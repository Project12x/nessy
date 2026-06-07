// NsfEngine.cpp — load() bus wiring (Phase B.1 Task 3)
// Reproduces NSFPlayer::Reload() bus topology from bbbradsmith/nsfplay (permissive).
// Upstream reference: xgm/player/nsf/nsfplay.cpp inspected at
//   commit 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (master, 2025-02-04)
// Reuse mode: pattern-only (same device-attach order; no code copied verbatim).
// First-party additions: unique_ptr ownership, no vcm::Config, no entangled extras.

#include "NsfEngine.h"
#include <cstring>
#include <cassert>

namespace nessy {

NsfEngine::NsfEngine()
    : cpu_(1789773.0) // NES_CPU constructor takes initial clock
{
    // Allocate the two always-present chips immediately so that
    // dmc->SetCPU / dmc->SetAPU can be called before load().
    apu_ = std::make_unique<xgm::NES_APU>();
    dmc_ = std::make_unique<xgm::NES_DMC>();

    // CRITICAL (see Phase B.1 hard-won lesson): DMC dereferences its CPU
    // and APU pointers unguarded inside Reset() and UpdateIRQ/StealCycles.
    // Wire these immediately so any accidental early Reset() does not crash.
    dmc_->SetCPU(&cpu_);
    dmc_->SetAPU(apu_.get());

    // NSF2 IRQ needs the CPU pointer too.
    nsf2_irq_.SetCPU(&cpu_);
}

NsfEngine::~NsfEngine() = default;

bool NsfEngine::load(const xgm::UINT8* data, xgm::UINT32 size)
{
    loaded_ = false;

    // --- Parse the NSF header ---
    // NSF::Load takes a non-const pointer; the data is not modified.
    if (!nsf_.Load(const_cast<xgm::UINT8*>(data), size))
        return false;

    wireBus();
    loaded_ = true;
    return true;
}

void NsfEngine::wireBus()
{
    // --- Determine if bankswitch is needed ---
    int bmax = 0;
    for (int i = 0; i < 8; ++i)
        if (bmax < nsf_.bankswitch[i])
            bmax = nsf_.bankswitch[i];
    banked_ = (bmax != 0);

    // --- Load ROM into memory devices ---
    // SetImage(data, load_address, size)
    mem_.SetImage(nsf_.body, nsf_.load_address, nsf_.bodysize);

    if (banked_)
    {
        bank_.SetImage(nsf_.body, nsf_.load_address, nsf_.bodysize);
        for (int i = 0; i < 8; ++i)
            bank_.SetBankDefault(static_cast<xgm::UINT8>(i + 8), nsf_.bankswitch[i]);
    }

    // --- Reset bus lists (Reload may be called more than once) ---
    stack_.DetachAll();
    layer_.DetachAll();
    mixer_.DetachAll();
    apu_bus_.DetachAll();

    // --- Expansion chips: allocate on first load, reuse on reload ---
    if (nsf_.use_mmc5 && !mmc5_)
    {
        mmc5_ = std::make_unique<xgm::NES_MMC5>();
        mmc5_->SetCPU(&cpu_); // MMC5 PCM read requires CPU access
    }
    if (nsf_.use_vrc6 && !vrc6_)  vrc6_ = std::make_unique<xgm::NES_VRC6>();
    if (nsf_.use_vrc7 && !vrc7_)  vrc7_ = std::make_unique<xgm::NES_VRC7>();
    if (nsf_.use_fme7 && !fme7_)  fme7_ = std::make_unique<xgm::NES_FME7>();
    if (nsf_.use_n106 && !n106_)  n106_ = std::make_unique<xgm::NES_N106>();
    if (nsf_.use_fds  && !fds_)   fds_  = std::make_unique<xgm::NES_FDS>();

    // --- Clock all active chips ---
    // (SetRate is not called here; Task 4 will call SetRate at render time.)
    apu_->SetClock(basecycles_);
    dmc_->SetClock(basecycles_);
    if (mmc5_)  mmc5_->SetClock(basecycles_);
    if (vrc6_)  vrc6_->SetClock(basecycles_);
    if (vrc7_)  vrc7_->SetClock(basecycles_);
    if (fme7_)  fme7_->SetClock(basecycles_);
    if (n106_)  n106_->SetClock(basecycles_);
    if (fds_)   fds_->SetClock(basecycles_);

    // --- NSF2 vector overlay (NSF2 IRQ / non-returning INIT) ---
    // Must be done before building the layer so ForceVector has the right mem_.
    if (nsf_.nsf2_bits & 0x30)
    {
        // NSF2: attach vector overlay into memory layer (read-only, matches $FFFA-$FFFF)
        layer_.Attach(&nsf2_vectors_);
        nsf2_vectors_.SetCPU(&cpu_);
        // NMI vector → NES player program NMI stub at PLAYER_RESERVED+0x06
        nsf2_vectors_.ForceVector(0, xgm::PLAYER_RESERVED + 0x06);
        // Reset vector → infinite idle loop PLAYER_RESERVED+0x03
        nsf2_vectors_.ForceVector(1, xgm::PLAYER_RESERVED + 0x03);
        // IRQ vector: read the existing vector from mem_ (or bank_ if banked)
        xgm::UINT32 ivlo = 0, ivhi = 0;
        if (banked_) { bank_.Read(0xFFFE, ivlo); bank_.Read(0xFFFF, ivhi); }
        else         { mem_.Read(0xFFFE, ivlo);  mem_.Read(0xFFFF, ivhi);  }
        xgm::UINT32 iva = (ivlo & 0xFF) | ((ivhi & 0xFF) << 8);
        nsf2_vectors_.ForceVector(2, iva);
    }

    if (nsf_.nsf2_bits & 0x10) // uses NSF2 IRQ timer
    {
        stack_.Attach(&nsf2_irq_);
    }

    // --- Memory sub-layer: [bank] [mem] ---
    if (banked_) layer_.Attach(&bank_);
    layer_.Attach(&mem_);

    // --- Wire DMC to the memory layer (needed for DPCM sample fetch) ---
    dmc_->SetMemory(&layer_);
    // Ensure CPU/APU pointers are still set after any chip re-creation.
    dmc_->SetCPU(&cpu_);
    dmc_->SetAPU(apu_.get());

    // --- APU bus: broadcast to NES_APU and NES_DMC ($4000-$4017) ---
    apu_bus_.Attach(apu_.get());
    apu_bus_.Attach(dmc_.get());

    // --- Build the top-level stack ---
    // Order from upstream Reload():
    //   [nsf2_irq (already attached above if needed)]
    //   [apu_bus]
    //   [MMC5] [VRC6] [VRC7] [FME7] [N106] [FDS]  (expansion chips, if active)
    //   [layer]  (last)
    stack_.Attach(&apu_bus_);

    if (mmc5_) stack_.Attach(mmc5_.get());
    if (vrc6_) stack_.Attach(vrc6_.get());
    if (vrc7_) stack_.Attach(vrc7_.get());
    if (fme7_) stack_.Attach(fme7_.get());
    if (n106_) stack_.Attach(n106_.get());
    if (fds_)  stack_.Attach(fds_.get());

    // Memory layer is always last in the stack.
    stack_.Attach(&layer_);

    // --- Build mixer: [amp_apu] [amp_dmc] [expansion amps...] ---
    amp_apu_.Attach(apu_.get());
    amp_dmc_.Attach(dmc_.get());
    mixer_.Attach(&amp_apu_);
    mixer_.Attach(&amp_dmc_);

    if (mmc5_) { amp_mmc5_.Attach(mmc5_.get()); mixer_.Attach(&amp_mmc5_); }
    if (vrc6_) { amp_vrc6_.Attach(vrc6_.get()); mixer_.Attach(&amp_vrc6_); }
    if (vrc7_) { amp_vrc7_.Attach(vrc7_.get()); mixer_.Attach(&amp_vrc7_); }
    if (fme7_) { amp_fme7_.Attach(fme7_.get()); mixer_.Attach(&amp_fme7_); }
    if (n106_) { amp_n106_.Attach(n106_.get()); mixer_.Attach(&amp_n106_); }
    if (fds_)  { amp_fds_.Attach(fds_.get());   mixer_.Attach(&amp_fds_);  }

    // --- Wire CPU to the top-level bus ---
    cpu_.SetMemory(&stack_);
    cpu_.SetNESMemory(&mem_);
    cpu_.SetLogger(nullptr);
}

// ---------------------------------------------------------------------------
// init() and renderSamples() — stubs, implemented in Task 4
// ---------------------------------------------------------------------------

void NsfEngine::init(int /*song*/)
{
    // TODO (Task 4): call stack_.Reset(), cpu_.Reset(), cpu_.Start(...)
}

void NsfEngine::renderSamples(int16_t* /*out*/, int /*count*/, double /*outputRate*/)
{
    // TODO (Task 4): tick CPU, call mixer_.Render(), resample to outputRate
}

} // namespace nessy
