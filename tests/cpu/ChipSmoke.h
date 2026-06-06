#pragma once
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include "xgm/devices/device.h"   // xgm::ISoundChip, INT32, UINT32

struct SmokeResult { bool nonSilent = false; bool bounded = true; };

inline SmokeResult smokeRender(xgm::ISoundChip& chip, int samples = 4000,
                               double clock = 1789772.7, double rate = 48000.0) {
    const int ticksPerSample = static_cast<int>(clock / rate);   // ~37
    SmokeResult r;
    for (int i = 0; i < samples; ++i) {
        chip.Tick(static_cast<xgm::UINT32>(ticksPerSample));
        xgm::INT32 b[2] = {0, 0};
        chip.Render(b);
        if (b[0] != 0 || b[1] != 0) r.nonSilent = true;
        if (std::llabs((long long)b[0]) > (1LL << 24) ||
            std::llabs((long long)b[1]) > (1LL << 24)) r.bounded = false;
    }
    return r;
}
