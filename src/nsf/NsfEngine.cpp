// NsfEngine.cpp — load() bus wiring (Phase B.1 Task 3)
// PIMPL-refactored Phase B.2 Task 0: all NSFPlay/km6502 types confined here so
// NsfEngine.h is safe to include alongside JUCE headers.
//
// Reproduces NSFPlayer::Reload() bus topology from bbbradsmith/nsfplay (permissive).
// Upstream reference: xgm/player/nsf/nsfplay.cpp inspected at
//   commit 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (master, 2025-02-04)
// Reuse mode: pattern-only (same device-attach order; no code copied verbatim).
// First-party additions: unique_ptr ownership, no vcm::Config, no entangled extras.

// NSFPlay headers first — they bring in km6502 macros.
#include "xgm/player/nsf/nsf.h"
#include "xgm/devices/CPU/nes_cpu.h"
#include "xgm/devices/Memory/nes_mem.h"
#include "xgm/devices/Memory/nes_bank.h"
#include "xgm/devices/Memory/nsf2_vectors.h"
#include "xgm/devices/Misc/nsf2_irq.h"
#include "xgm/devices/Sound/nes_apu.h"
#include "xgm/devices/Sound/nes_dmc.h"
#include "xgm/devices/Sound/nes_vrc6.h"
#include "xgm/devices/Sound/nes_vrc7.h"
#include "xgm/devices/Sound/nes_fme7.h"
#include "xgm/devices/Sound/nes_mmc5.h"
#include "xgm/devices/Sound/nes_n106.h"
#include "xgm/devices/Sound/nes_fds.h"
#include "xgm/devices/Audio/mixer.h"
#include "xgm/devices/Audio/amplifier.h"
#include "xgm/devices/device.h"

// Purge km6502 macros so subsequent standard/JUCE headers are not polluted.
#undef External
#undef Callback
#undef Inline
#undef CCall
#undef FastCall

#include "NsfEngine.h"
#include <cstring>
#include <cassert>

namespace nessy {

// ---------------------------------------------------------------------------
// Impl — all former value members live here
// ---------------------------------------------------------------------------
struct NsfEngine::Impl
{
    // --- Parsed NSF data ---
    xgm::NSF        nsf;

    // --- CPU + memory ---
    xgm::NES_CPU    cpu;
    xgm::NES_MEM    mem;
    xgm::NES_BANK   bank;
    xgm::NSF2_Vectors nsf2_vectors;
    xgm::NSF2_IRQ   nsf2_irq;

    // --- Bus structures (first-match Layer / broadcast Bus) ---
    xgm::Bus        apu_bus;   // broadcast to NES_APU + NES_DMC
    xgm::Layer      stack;     // top-level CPU memory bus
    xgm::Layer      layer;     // memory sub-layer (bank + RAM)

    // --- Mixer for rendering ---
    xgm::Mixer      mixer;

    // --- Core chips (always present) ---
    std::unique_ptr<xgm::NES_APU>  apu;
    std::unique_ptr<xgm::NES_DMC>  dmc;

    // --- Amplifiers (one per chip slot) ---
    // Order matches DeviceCode enum: APU, DMC, FME7, MMC5, N106, VRC6, VRC7, FDS
    xgm::Amplifier  amp_apu;
    xgm::Amplifier  amp_dmc;
    xgm::Amplifier  amp_fme7;
    xgm::Amplifier  amp_mmc5;
    xgm::Amplifier  amp_n106;
    xgm::Amplifier  amp_vrc6;
    xgm::Amplifier  amp_vrc7;
    xgm::Amplifier  amp_fds;

    // --- Expansion chips (null when not active) ---
    std::unique_ptr<xgm::NES_VRC6> vrc6;
    std::unique_ptr<xgm::NES_VRC7> vrc7;
    std::unique_ptr<xgm::NES_FME7> fme7;
    std::unique_ptr<xgm::NES_MMC5> mmc5;
    std::unique_ptr<xgm::NES_N106> n106;
    std::unique_ptr<xgm::NES_FDS>  fds;

    // --- State ---
    bool   banked       = false;
    double basecycles   = 1789773.0; // NTSC CPU clock
    double clock_rest   = 0.0;
    bool   loaded       = false;

    Impl() : cpu(1789773.0) // NES_CPU constructor takes initial clock
    {
        // Allocate the two always-present chips immediately so that
        // dmc->SetCPU / dmc->SetAPU can be called before load().
        apu = std::make_unique<xgm::NES_APU>();
        dmc = std::make_unique<xgm::NES_DMC>();

        // CRITICAL (see Phase B.1 hard-won lesson): DMC dereferences its CPU
        // and APU pointers unguarded inside Reset() and UpdateIRQ/StealCycles.
        // Wire these immediately so any accidental early Reset() does not crash.
        dmc->SetCPU(&cpu);
        dmc->SetAPU(apu.get());

        // NSF2 IRQ needs the CPU pointer too.
        nsf2_irq.SetCPU(&cpu);
    }
};

// ---------------------------------------------------------------------------
// NsfEngine public API
// ---------------------------------------------------------------------------

NsfEngine::NsfEngine()
    : impl_(std::make_unique<Impl>())
{
}

NsfEngine::~NsfEngine() = default;

bool NsfEngine::load(const uint8_t* data, size_t size)
{
    impl_->loaded = false;

    // --- Parse the NSF header ---
    // NSF::Load takes a non-const pointer; the data is not modified.
    if (!impl_->nsf.Load(const_cast<xgm::UINT8*>(data), static_cast<xgm::UINT32>(size)))
        return false;

    wireBus();
    impl_->loaded = true;
    return true;
}

// Private helper — defined here where Impl is complete.
void NsfEngine::wireBus()
{
    auto& p = *impl_;

    // --- Determine if bankswitch is needed ---
    int bmax = 0;
    for (int i = 0; i < 8; ++i)
        if (bmax < p.nsf.bankswitch[i])
            bmax = p.nsf.bankswitch[i];
    p.banked = (bmax != 0);

    // --- Load ROM into memory devices ---
    // SetImage(data, load_address, size)
    p.mem.SetImage(p.nsf.body, p.nsf.load_address, p.nsf.bodysize);

    if (p.banked)
    {
        p.bank.SetImage(p.nsf.body, p.nsf.load_address, p.nsf.bodysize);
        for (int i = 0; i < 8; ++i)
            p.bank.SetBankDefault(static_cast<xgm::UINT8>(i + 8), p.nsf.bankswitch[i]);
    }

    // --- Reset bus lists (Reload may be called more than once) ---
    p.stack.DetachAll();
    p.layer.DetachAll();
    p.mixer.DetachAll();
    p.apu_bus.DetachAll();

    // --- Expansion chips: allocate on first load, reuse on reload ---
    if (p.nsf.use_mmc5 && !p.mmc5)
    {
        p.mmc5 = std::make_unique<xgm::NES_MMC5>();
        p.mmc5->SetCPU(&p.cpu); // MMC5 PCM read requires CPU access
    }
    if (p.nsf.use_vrc6 && !p.vrc6)  p.vrc6 = std::make_unique<xgm::NES_VRC6>();
    if (p.nsf.use_vrc7 && !p.vrc7)  p.vrc7 = std::make_unique<xgm::NES_VRC7>();
    if (p.nsf.use_fme7 && !p.fme7)  p.fme7 = std::make_unique<xgm::NES_FME7>();
    if (p.nsf.use_n106 && !p.n106)  p.n106 = std::make_unique<xgm::NES_N106>();
    if (p.nsf.use_fds  && !p.fds)   p.fds  = std::make_unique<xgm::NES_FDS>();

    // --- Clock all active chips ---
    // (SetRate is not called here; renderSamples() calls SetRate at render time.)
    p.apu->SetClock(p.basecycles);
    p.dmc->SetClock(p.basecycles);
    if (p.mmc5) p.mmc5->SetClock(p.basecycles);
    if (p.vrc6) p.vrc6->SetClock(p.basecycles);
    if (p.vrc7) p.vrc7->SetClock(p.basecycles);
    if (p.fme7) p.fme7->SetClock(p.basecycles);
    if (p.n106) p.n106->SetClock(p.basecycles);
    if (p.fds)  p.fds->SetClock(p.basecycles);

    // --- NSF2 vector overlay (NSF2 IRQ / non-returning INIT) ---
    // Must be done before building the layer so ForceVector has the right mem.
    if (p.nsf.nsf2_bits & 0x30)
    {
        // NSF2: attach vector overlay into memory layer (read-only, matches $FFFA-$FFFF)
        p.layer.Attach(&p.nsf2_vectors);
        p.nsf2_vectors.SetCPU(&p.cpu);
        // NMI vector → NES player program NMI stub at PLAYER_RESERVED+0x06
        p.nsf2_vectors.ForceVector(0, xgm::PLAYER_RESERVED + 0x06);
        // Reset vector → infinite idle loop PLAYER_RESERVED+0x03
        p.nsf2_vectors.ForceVector(1, xgm::PLAYER_RESERVED + 0x03);
        // IRQ vector: read the existing vector from mem (or bank if banked)
        xgm::UINT32 ivlo = 0, ivhi = 0;
        if (p.banked) { p.bank.Read(0xFFFE, ivlo); p.bank.Read(0xFFFF, ivhi); }
        else          { p.mem.Read(0xFFFE, ivlo);  p.mem.Read(0xFFFF, ivhi);  }
        xgm::UINT32 iva = (ivlo & 0xFF) | ((ivhi & 0xFF) << 8);
        p.nsf2_vectors.ForceVector(2, iva);
    }

    if (p.nsf.nsf2_bits & 0x10) // uses NSF2 IRQ timer
    {
        p.stack.Attach(&p.nsf2_irq);
    }

    // --- Memory sub-layer: [bank] [mem] ---
    if (p.banked) p.layer.Attach(&p.bank);
    p.layer.Attach(&p.mem);

    // --- Wire DMC to the memory layer (needed for DPCM sample fetch) ---
    p.dmc->SetMemory(&p.layer);
    // Ensure CPU/APU pointers are still set after any chip re-creation.
    p.dmc->SetCPU(&p.cpu);
    p.dmc->SetAPU(p.apu.get());

    // --- APU bus: broadcast to NES_APU and NES_DMC ($4000-$4017) ---
    p.apu_bus.Attach(p.apu.get());
    p.apu_bus.Attach(p.dmc.get());

    // --- Build the top-level stack ---
    // Order from upstream Reload():
    //   [nsf2_irq (already attached above if needed)]
    //   [apu_bus]
    //   [MMC5] [VRC6] [VRC7] [FME7] [N106] [FDS]  (expansion chips, if active)
    //   [layer]  (last)
    p.stack.Attach(&p.apu_bus);

    if (p.mmc5) p.stack.Attach(p.mmc5.get());
    if (p.vrc6) p.stack.Attach(p.vrc6.get());
    if (p.vrc7) p.stack.Attach(p.vrc7.get());
    if (p.fme7) p.stack.Attach(p.fme7.get());
    if (p.n106) p.stack.Attach(p.n106.get());
    if (p.fds)  p.stack.Attach(p.fds.get());

    // Memory layer is always last in the stack.
    p.stack.Attach(&p.layer);

    // --- Build mixer: [amp_apu] [amp_dmc] [expansion amps...] ---
    p.amp_apu.Attach(p.apu.get());
    p.amp_dmc.Attach(p.dmc.get());
    p.mixer.Attach(&p.amp_apu);
    p.mixer.Attach(&p.amp_dmc);

    if (p.mmc5) { p.amp_mmc5.Attach(p.mmc5.get()); p.mixer.Attach(&p.amp_mmc5); }
    if (p.vrc6) { p.amp_vrc6.Attach(p.vrc6.get()); p.mixer.Attach(&p.amp_vrc6); }
    if (p.vrc7) { p.amp_vrc7.Attach(p.vrc7.get()); p.mixer.Attach(&p.amp_vrc7); }
    if (p.fme7) { p.amp_fme7.Attach(p.fme7.get()); p.mixer.Attach(&p.amp_fme7); }
    if (p.n106) { p.amp_n106.Attach(p.n106.get()); p.mixer.Attach(&p.amp_n106); }
    if (p.fds)  { p.amp_fds.Attach(p.fds.get());   p.mixer.Attach(&p.amp_fds);  }

    // --- Wire CPU to the top-level bus ---
    p.cpu.SetMemory(&p.stack);
    p.cpu.SetNESMemory(&p.mem);
    p.cpu.SetLogger(nullptr);
}

// ---------------------------------------------------------------------------
// init() and renderSamples()
// ---------------------------------------------------------------------------

// Player stub program placed at PLAYER_RESERVED ($4100):
//   $4100: 20 xx xx  JSR <target>  (bytes 1-2 are patched by run_from/WriteReserved)
//   $4103: 4C 03 41  JMP $4103     (breakpoint — self-loop after JSR returns)
//   $4106+: RTI stubs for NMI/IRQ used by NSF2; harmless for NSF1
//
// Upstream reference: bbbradsmith/nsfplay xgm/player/nsf/nsfplay.cpp
//   PLAYER_PROGRAM bytes, inspected at commit 6af5406 (master, 2025-02-04).
// Reuse mode: pattern-only.
static constexpr xgm::UINT8 kPlayerProgram[] = {
    0x20, 0x00, 0x00,  // JSR $0000 — target patched at runtime by run_from()
    0x4C, 0x03, 0x41,  // JMP $4103 — self-loop / breakpoint
    0x40,              // RTI — NMI handler stub  (PLAYER_RESERVED+0x06)
    0x40,              // RTI — IRQ handler stub  (PLAYER_RESERVED+0x07)
};

void NsfEngine::init(int song)
{
    auto& p = *impl_;

    // Reset all attached devices on the bus (including APU, DMC, expansion chips).
    // stack.Reset() broadcasts to every IDevice attached to the Layer/Bus chain.
    p.stack.Reset();
    p.cpu.Reset();

    // Install player stub into the reserved $4100 region.
    // Must be done after cpu.Reset() because Reset() clears the context but
    // does not touch the NES_MEM image; the stub remains valid across Reset.
    p.mem.SetReserved(kPlayerProgram, static_cast<xgm::UINT32>(sizeof(kPlayerProgram)));

    // Reclock chips — stack.Reset() may have cleared internal state.
    p.apu->SetClock(p.basecycles);
    p.dmc->SetClock(p.basecycles);
    if (p.mmc5) p.mmc5->SetClock(p.basecycles);
    if (p.vrc6) p.vrc6->SetClock(p.basecycles);
    if (p.vrc7) p.vrc7->SetClock(p.basecycles);
    if (p.fme7) p.fme7->SetClock(p.basecycles);
    if (p.n106) p.n106->SetClock(p.basecycles);
    if (p.fds)  p.fds->SetClock(p.basecycles);

    p.clock_rest = 0.0;

    // NTSC play rate: speed_ntsc is microseconds per frame; 0 treated as 16639.
    double playRateHz = 1000000.0 / (p.nsf.speed_ntsc ? p.nsf.speed_ntsc : 16639);

    // Start drains INIT to completion and leaves the CPU resting in the
    // $4103 self-loop (breaked). Song is 0-based; region 0 = NTSC.
    p.cpu.Start(p.nsf.init_address, p.nsf.play_address, playRateHz,
                song, /*region NTSC*/ 0,
                p.nsf.nsf2_bits, /*enable_irq*/ false, &p.nsf2_irq);
}

void NsfEngine::renderSamples(int16_t* out, int count, double outputRate)
{
    auto& p = *impl_;

    // SetRate on every active chip once per render call.
    p.apu->SetRate(outputRate);
    p.dmc->SetRate(outputRate);
    if (p.mmc5) p.mmc5->SetRate(outputRate);
    if (p.vrc6) p.vrc6->SetRate(outputRate);
    if (p.vrc7) p.vrc7->SetRate(outputRate);
    if (p.fme7) p.fme7->SetRate(outputRate);
    if (p.n106) p.n106->SetRate(outputRate);
    if (p.fds)  p.fds->SetRate(outputRate);

    for (int i = 0; i < count; ++i)
    {
        // Accumulate fractional CPU clocks: ~40.6 cycles per sample at 44.1 kHz.
        p.clock_rest += p.basecycles / outputRate;
        int clk = static_cast<int>(p.clock_rest);
        p.clock_rest -= clk;

        // Run the 6502 for clk CPU clocks; PLAY fires internally each NTSC frame.
        p.cpu.Exec(clk);

        // Tick all sound chips via the mixer's broadcast.
        // Mixer::Tick iterates all attached IRenderable amplifiers and calls Tick.
        p.mixer.Tick(static_cast<xgm::UINT32>(clk));

        // Render: mixer sums all amplifier outputs into b[0]/b[1] (L/R).
        xgm::INT32 b[2] = {0, 0};
        p.mixer.Render(b);

        // Clamp and write mono (L channel); stereo output is a future concern.
        long v = b[0];
        if (v > 32767)  v = 32767;
        else if (v < -32768) v = -32768;
        out[i] = static_cast<int16_t>(v);
    }
}

// ---------------------------------------------------------------------------
// Metadata accessors
// ---------------------------------------------------------------------------

unsigned NsfEngine::cpuPC() const
{
    return impl_->cpu.GetPC();
}

std::string NsfEngine::title() const
{
    // nsf.title points to title_nsf (or the NSFe title if present).
    if (impl_->nsf.title && impl_->nsf.title[0] != '\0')
        return std::string(impl_->nsf.title);
    return std::string(impl_->nsf.title_nsf);
}

int NsfEngine::songCount() const
{
    return static_cast<int>(impl_->nsf.songs);
}

} // namespace nessy
