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
    int songCount() const;

private:
    void wireBus(); // implemented in NsfEngine.cpp where Impl is complete

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nessy
