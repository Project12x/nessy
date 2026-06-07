#pragma once
// NsfEngine.h — slim NSF machine (CPU + memory + expansion bus)
// First-party Nessy code, Phase B.1 Task 3.
// Reproduces the NSFPlayer bus topology from bbbradsmith/nsfplay (permissive)
// minus entangled extras (loop detector, CPULogger, Fader, Filter, RateConverter,
// vcm::Config, NESDetector).
//
// Upstream reference: bbbradsmith/nsfplay xgm/player/nsf/nsfplay.cpp NSFPlayer::Reload()
// Inspected at commit 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (master, 2025-02-04)
// License: permissive (zlib-style per nsfplay README)

#include <cstdint>
#include <memory>

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

namespace nessy {

class NsfEngine
{
public:
    NsfEngine();
    ~NsfEngine();

    // Parse NSF data and wire the bus. Returns false if parse fails.
    bool load(const xgm::UINT8* data, xgm::UINT32 size);

    // INIT routine for the given song (0-based). Stub — implemented in Task 4.
    void init(int song = 0);

    // Render 'count' stereo int16 samples at outputRate Hz. Stub — Task 4.
    void renderSamples(int16_t* out, int count, double outputRate);

    // Current CPU program counter.
    unsigned cpuPC() const { return cpu_.GetPC(); }

    // Access the parsed NSF metadata.
    const xgm::NSF& nsf() const { return nsf_; }

private:
    void wireBus();

    // --- Parsed NSF data ---
    xgm::NSF        nsf_;

    // --- CPU + memory ---
    xgm::NES_CPU    cpu_;
    xgm::NES_MEM    mem_;
    xgm::NES_BANK   bank_;
    xgm::NSF2_Vectors nsf2_vectors_;
    xgm::NSF2_IRQ   nsf2_irq_;

    // --- Bus structures (first-match Layer / broadcast Bus) ---
    xgm::Bus        apu_bus_;   // broadcast to NES_APU + NES_DMC
    xgm::Layer      stack_;     // top-level CPU memory bus
    xgm::Layer      layer_;     // memory sub-layer (bank + RAM)

    // --- Mixer for rendering ---
    xgm::Mixer      mixer_;

    // --- Core chips (always present) ---
    std::unique_ptr<xgm::NES_APU>  apu_;
    std::unique_ptr<xgm::NES_DMC>  dmc_;

    // --- Amplifiers (one per chip slot) ---
    // Order matches DeviceCode enum: APU, DMC, FME7, MMC5, N106, VRC6, VRC7, FDS
    xgm::Amplifier  amp_apu_;
    xgm::Amplifier  amp_dmc_;
    xgm::Amplifier  amp_fme7_;
    xgm::Amplifier  amp_mmc5_;
    xgm::Amplifier  amp_n106_;
    xgm::Amplifier  amp_vrc6_;
    xgm::Amplifier  amp_vrc7_;
    xgm::Amplifier  amp_fds_;

    // --- Expansion chips (null when not active) ---
    std::unique_ptr<xgm::NES_VRC6> vrc6_;
    std::unique_ptr<xgm::NES_VRC7> vrc7_;
    std::unique_ptr<xgm::NES_FME7> fme7_;
    std::unique_ptr<xgm::NES_MMC5> mmc5_;
    std::unique_ptr<xgm::NES_N106> n106_;
    std::unique_ptr<xgm::NES_FDS>  fds_;

    // --- State ---
    bool   banked_       = false;
    double basecycles_   = 1789773.0; // NTSC CPU clock
    double clock_rest_   = 0.0;
    bool   loaded_       = false;
};

} // namespace nessy
