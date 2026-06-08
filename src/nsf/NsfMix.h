#pragma once
// NsfMix.h — Mono int16 -> stereo float conversion for the NSF playback path.
// Extracted as a free function so it can be unit-tested independently of the
// JUCE AudioProcessor (Phase B.2 Task 6).

#include <cstdint>

// Convert 'n' mono int16 samples to stereo float L/R buffers.
// L and R may point to the same buffer (mono output); function handles that case.
// Scale: 32768 -> 1.0f  (full-scale NES output maps to 0 dBFS).
inline void nsfMonoToStereo(const int16_t* mono, float* L, float* R, int n) {
    const float k = 1.0f / 32768.0f;
    if (L == R) {
        for (int i = 0; i < n; ++i)
            L[i] = (float)mono[i] * k;
    } else {
        for (int i = 0; i < n; ++i) {
            float s = (float)mono[i] * k;
            L[i] = s;
            R[i] = s;
        }
    }
}
