#include <catch2/catch_test_macros.hpp>
#include "ChannelRegistry.h"

using namespace nessy;

static int countMelodic() {
  int n = 0;
  for (auto& c : kChannels)
    if (c.role == ChannelRole::Melodic) ++n;
  return n;
}

TEST_CASE("channel registry has the expected current layout", "[voicealloc][registry]") {
  REQUIRE(kChannels.size() == 8);
  REQUIRE(countMelodic() == 6);                     // P1 P2 Tri VRC6x3

  // Core 2A03 has 3 melodic (P1,P2,Tri) + 2 percussion (Noise,DMC).
  int core = 0, corePerc = 0;
  for (auto& c : kChannels)
    if (c.group == ChipGroup::Core2A03) {
      ++core;
      if (c.role == ChannelRole::Percussion) ++corePerc;
    }
  REQUIRE(core == 5);
  REQUIRE(corePerc == 2);

  // Bass tier = Triangle (id 2) + VRC6 Saw (id 7).
  REQUIRE(kChannels[2].splitTier == SplitTier::Bass);
  REQUIRE(kChannels[7].splitTier == SplitTier::Bass);
  REQUIRE(kChannels[0].splitTier == SplitTier::Lead);

  // Percussion rows carry no split tier.
  REQUIRE(kChannels[3].splitTier == SplitTier::None);  // Noise
  REQUIRE(kChannels[4].splitTier == SplitTier::None);  // DMC

  // Pin the id <-> row-index mapping the allocator relies on (m_channels[pos].id
  // is written straight into NessyAPU note calls in Task 3).
  for (int i = 0; i < static_cast<int>(kChannels.size()); ++i)
    REQUIRE(kChannels[i].id == i);
}
