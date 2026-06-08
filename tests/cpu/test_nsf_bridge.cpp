// test_nsf_bridge.cpp — unit tests for the NSF mono->stereo bridge (NsfMix.h).
// Phase B.2 Task 6.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NsfMix.h"

TEST_CASE("nsfMonoToStereo basic conversion", "[nsf][bridge]") {
    const int16_t mono[3] = { 32767, -32768, 0 };
    float L[3] = {};
    float R[3] = {};

    nsfMonoToStereo(mono, L, R, 3);

    // L and R must be identical (true stereo copy of mono signal).
    REQUIRE(L[0] == R[0]);
    REQUIRE(L[1] == R[1]);
    REQUIRE(L[2] == R[2]);

    // Amplitude checks: 32767/32768 ~ 0.9999694
    REQUIRE(L[0] == Catch::Approx(32767.0f / 32768.0f).epsilon(1e-5));
    // -32768/32768 = -1.0 exactly
    REQUIRE(L[1] == Catch::Approx(-1.0f).epsilon(1e-7));
    // 0 -> 0.0
    REQUIRE(L[2] == 0.0f);
}

TEST_CASE("nsfMonoToStereo single-pointer (mono) path", "[nsf][bridge]") {
    // When L == R the function takes the single-pointer fast path; results
    // must match the normal two-pointer path.
    const int16_t mono[3] = { 32767, -32768, 0 };
    float buf[3] = {};

    nsfMonoToStereo(mono, buf, buf, 3);

    REQUIRE(buf[0] == Catch::Approx(32767.0f / 32768.0f).epsilon(1e-5));
    REQUIRE(buf[1] == Catch::Approx(-1.0f).epsilon(1e-7));
    REQUIRE(buf[2] == 0.0f);
}
