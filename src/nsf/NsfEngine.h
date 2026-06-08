#pragma once
// NsfEngine.h — slim NSF machine (CPU + memory + expansion bus)
// First-party Nessy code, Phase B.1 Task 3; PIMPL-refactored Phase B.2 Task 0.
// Reproduces the NSFPlayer bus topology from bbbradsmith/nsfplay (permissive)
// minus entangled extras (loop detector, CPULogger, Fader, Filter, RateConverter,
// vcm::Config, NESDetector).
//
// Upstream reference: bbbradsmith/nsfplay xgm/player/nsf/nsfplay.cpp NSFPlayer::Reload()
// Inspected at commit 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (master, 2025-02-04)
// License: permissive (zlib-style per nsfplay README)
//
// PIMPL: all NSFPlay/km6502 types are confined to NsfEngine.cpp so this header
// is safe to include alongside JUCE headers (no km6502 macro pollution).

#include <cstdint>
#include <memory>
#include <string>

namespace nessy {

class NsfEngine
{
public:
    NsfEngine();
    ~NsfEngine();

    // Parse NSF data and wire the bus. Returns false if parse fails.
    bool load(const uint8_t* data, size_t size);

    // INIT routine for the given song (0-based).
    void init(int song = 0);

    // Render 'count' stereo int16 samples at outputRate Hz.
    void renderSamples(int16_t* out, int count, double outputRate);

    // Current CPU program counter.
    unsigned cpuPC() const;

    // Metadata (read after a successful load).
    std::string title() const;
    std::string artist() const;
    std::string copyright() const;
    // Chip list: always "2A03", plus " + VRC6" / " + VRC7" / " + FDS" /
    //            " + MMC5" / " + N163" / " + 5B" for each active expansion.
    std::string chips() const;
    int songCount() const;

    // Per-channel scope ring buffers — lock-free, written by audio thread,
    // read by message thread (same benign pattern as NessyAPU::m_visualizerBuffers).
    // Channels: 0=P1, 1=P2, 2=TRI, 3=NSE, 4=DMC.
    static constexpr int kScopeChannels   = 5;
    static constexpr int kScopeBufferSize = 512;
    // Returns a pointer to the 512-float ring buffer for channel ch,
    // or nullptr if ch is out of range. Header stays NSFPlay-symbol-free (PIMPL).
    const float* scopeBuffer(int ch) const;

private:
    void wireBus(); // implemented in NsfEngine.cpp where Impl is complete

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nessy
