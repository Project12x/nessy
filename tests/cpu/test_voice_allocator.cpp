#include <catch2/catch_test_macros.hpp>
#include "ChannelRegistry.h"
#include "VoiceAllocator.h"
#include "MockVoiceSink.h"
#include <array>

using namespace nessy;

static int countMelodic() {
  int n = 0;
  for (auto& c : kChannels)
    if (c.role == ChannelRole::Melodic) ++n;
  return n;
}

TEST_CASE("channel registry has the expected current layout", "[voicealloc][registry]") {
  REQUIRE(kChannels.size() == 13);
  REQUIRE(countMelodic() == 11);                    // 6 prior + MMC5x2 + 5Bx3

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

  // MMC5 group: 2 melodic squares at ids 8,9.
  REQUIRE(kChannels[8].group == ChipGroup::MMC5);
  REQUIRE(kChannels[8].kind  == ChannelKind::Square);
  REQUIRE(kChannels[8].role  == ChannelRole::Melodic);
  REQUIRE(kChannels[9].group == ChipGroup::MMC5);
  // FME7 group: 3 melodic squares at ids 10,11,12.
  REQUIRE(kChannels[10].group == ChipGroup::FME7);
  REQUIRE(kChannels[12].group == ChipGroup::FME7);
  REQUIRE(kChannels[11].splitTier == SplitTier::Lead);

  // Pin the id <-> row-index mapping the allocator relies on (m_channels[pos].id
  // is written straight into NessyAPU note calls in Task 3).
  for (int i = 0; i < static_cast<int>(kChannels.size()); ++i)
    REQUIRE(kChannels[i].id == i);
}

// A synthetic N>8 registry: 10 melodic "lead" squares (ids 0..9) in one test
// group, to prove the allocator generalizes past the legacy 8 channels.
static constexpr std::array<ChannelDesc, 10> kTenLeads = {{
    {0, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {1, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {2, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {3, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {4, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {5, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {6, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {7, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {8, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
    {9, ChipGroup::MMC5, ChannelKind::Square, ChannelRole::Melodic, SplitTier::Lead},
}};

// Build an allocator on the production registry, Core2A03 only (VRC6 off).
static VoiceAllocator makeCoreAllocator(MockVoiceSink& sink) {
  VoiceAllocator va;
  va.setChannels(kChannels.data(), kChannels.size());
  va.setAPU(&sink);
  va.setVRC6Enabled(false);
  return va;
}

TEST_CASE("round-robin fills melodic channels in registry order, then steals oldest",
          "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  va.noteOn(0, 60, 1.0f);   // -> id 0 (P1)
  va.noteOn(0, 62, 1.0f);   // -> id 1 (P2)
  va.noteOn(0, 64, 1.0f);   // -> id 2 (Tri)
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2});

  sink.clear();
  va.noteOn(0, 65, 1.0f);   // all full -> steal oldest (id 0)
  REQUIRE(sink.offs == std::vector<int>{0});
  REQUIRE(sink.onChannels() == std::vector<int>{0});
}

TEST_CASE("VRC6 group enable extends the round-robin pool to 6", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setVRC6Enabled(true);
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  for (int n = 60; n < 66; ++n) va.noteOn(0, n, 1.0f);  // 6 distinct notes
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2, 5, 6, 7});
}

TEST_CASE("pitch-split routes low notes to bass tier, high notes to lead tier",
          "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setVRC6Enabled(true);
  va.setMode(VoiceAllocator::Mode::PITCH_SPLIT);
  va.setSplitPoint(60);

  va.noteOn(0, 48, 1.0f);   // low -> bass: Tri (id 2)
  va.noteOn(0, 50, 1.0f);   // low -> bass: VRC6 Saw (id 7)
  va.noteOn(0, 72, 1.0f);   // high -> lead: P1 (id 0)
  va.noteOn(0, 74, 1.0f);   // high -> lead: P2 (id 1)
  REQUIRE(sink.lastOnChannelFor(48) == 2);
  REQUIRE(sink.lastOnChannelFor(50) == 7);
  REQUIRE(sink.lastOnChannelFor(72) == 0);
  REQUIRE(sink.lastOnChannelFor(74) == 1);
}

TEST_CASE("unison triggers every enabled melodic channel; respects per-channel disable",
          "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setMode(VoiceAllocator::Mode::UNISON);

  va.noteOn(0, 60, 1.0f);   // P1,P2,Tri
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2});

  sink.clear();
  sink.disabled.insert(1);  // disable P2
  va.noteOff(0, 60);
  va.noteOn(0, 67, 1.0f);   // P1,Tri only
  REQUIRE(sink.onChannels() == std::vector<int>{0, 2});
}

TEST_CASE("noise and DMC are never melodically allocated", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  for (int n = 60; n < 70; ++n) va.noteOn(0, n, 1.0f);  // overflow the pool
  for (int ch : sink.onChannels()) {
    REQUIRE(ch != 3);   // Noise
    REQUIRE(ch != 4);   // DMC
  }
}

TEST_CASE("allocator generalizes past 8 channels (N=10)", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va;
  va.setChannels(kTenLeads.data(), kTenLeads.size());
  va.setAPU(&sink);
  va.setGroupEnabled(ChipGroup::MMC5, true);
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  for (int n = 60; n < 70; ++n) va.noteOn(0, n, 1.0f);  // fill all 10
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  sink.clear();
  va.noteOn(0, 70, 1.0f);   // 11th note -> steal oldest (id 0)
  REQUIRE(sink.offs == std::vector<int>{0});
  REQUIRE(sink.onChannels() == std::vector<int>{0});
}

TEST_CASE("pitch-split steals the oldest channel within a full tier", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setVRC6Enabled(true);
  va.setMode(VoiceAllocator::Mode::PITCH_SPLIT);
  va.setSplitPoint(60);

  va.noteOn(0, 48, 1.0f);   // low -> bass free: Tri (id 2)  [oldest]
  va.noteOn(0, 50, 1.0f);   // low -> bass free: VRC6 Saw (id 7)
  sink.clear();
  va.noteOn(0, 52, 1.0f);   // low -> bass full -> steal oldest bass (id 2)
  REQUIRE(sink.offs == std::vector<int>{2});
  REQUIRE(sink.lastOnChannelFor(52) == 2);
}

TEST_CASE("enabling the MMC5 group adds its channels to the pool", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va;
  va.setChannels(nessy::kChannels.data(), nessy::kChannels.size());
  va.setAPU(&sink);
  va.setGroupEnabled(nessy::ChipGroup::MMC5, true);   // Core2A03 (0,1,2) + MMC5 (8,9)
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  for (int n = 60; n < 65; ++n) va.noteOn(0, n, 1.0f); // 5 notes -> 0,1,2,8,9
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2, 8, 9});
}

TEST_CASE("noteOff releases the channel holding that note", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va = makeCoreAllocator(sink);
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  va.noteOn(0, 60, 1.0f);   // id 0
  va.noteOn(0, 62, 1.0f);   // id 1
  sink.clear();
  va.noteOff(0, 60);        // must release id 0 only
  REQUIRE(sink.offs == std::vector<int>{0});
  REQUIRE(va.getChannelForNote(60) == -1);
  REQUIRE(va.getChannelForNote(62) == 1);
}
