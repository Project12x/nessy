#include <catch2/catch_test_macros.hpp>
#include "SyntheticNsf.h"
#include "xgm/player/nsf/nsf.h"

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
