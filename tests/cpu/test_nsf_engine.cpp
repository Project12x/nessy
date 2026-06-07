#include <catch2/catch_test_macros.hpp>
#include "SyntheticNsf.h"
#include "xgm/player/nsf/nsf.h"
#include "NsfEngine.h"

TEST_CASE("NSF parses the synthetic header", "[nsf][parse]") {
    xgm::NSF nsf;
    REQUIRE(nsf.Load(const_cast<xgm::UINT8*>(kSyntheticNsf), (xgm::UINT32)sizeof(kSyntheticNsf)));
    REQUIRE(nsf.load_address == 0x8000);
    REQUIRE(nsf.init_address == 0x8000);
    REQUIRE(nsf.play_address == 0x801A);
    REQUIRE(nsf.songs == 1);
    REQUIRE(nsf.soundchip == 0);
    REQUIRE(nsf.bankswitch[0] == 0);
}

TEST_CASE("NsfEngine runs INIT and rests in the player loop", "[nsf][init]") {
    nessy::NsfEngine eng;
    REQUIRE(eng.load(kSyntheticNsf, (xgm::UINT32)sizeof(kSyntheticNsf)));
    eng.init(0);
    // After Start() drains INIT, the CPU rests at the player-stub self-loop.
    // PLAYER_RESERVED = $4100; breakpoint = $4103 (JMP $4103 — the self-loop).
    REQUIRE(eng.cpuPC() == 0x4103);
}
