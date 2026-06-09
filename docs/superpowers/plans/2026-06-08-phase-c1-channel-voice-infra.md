# Phase C.1 — Channel/Voice Infra Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generalize Nessy's `VoiceAllocator` from a hardcoded 8-channel layout to a data-driven N-channel pool (driven by a channel registry), with zero new chip audio, params, or UI, and prove it with a Catch2 unit suite — so later sub-phases can add expansion-chip voices by adding registry rows.

**Architecture:** Introduce a `constexpr` **channel registry** (`ChannelDesc` rows: id, chip group, kind, melodic/percussion role, split tier) as the single source of truth. The allocator iterates the registry filtered to the **active set** (enabled chip groups ∩ melodic) for round-robin / pitch-split / unison / voice-steal. Decouple the allocator from the concrete `NessyAPU` via a 3-method `IVoiceSink` interface (which `NessyAPU` already satisfies), so the allocator unit-tests link no JUCE/NSFPlay. With only the Core 2A03 + VRC6 groups enabled (today's set), allocation is byte-for-byte behavior-identical to the current code.

**Tech Stack:** C++20, JUCE 8 (plugin only — not the tests), Catch2 v3.5.2 + CTest, CMake + CPM, Windows/MSVC.

---

## Behavior-preservation invariant (why this is safe)

The current allocator uses `m_channelOrder = {0,1,2,5,6,7}` and `getMaxChannels() = vrc6 ? 6 : 3`. Iterating the registry **in row order, filtered to `role==Melodic && groupEnabled(group)`** yields:
- VRC6 off → ids `{0,1,2}` (P1,P2,Tri) — matches `channelOrder[0..2]`.
- VRC6 on → ids `{0,1,2,5,6,7}` — matches `channelOrder[0..5]`.

Pitch-split today routes low→`{Tri, VRC6_Saw}`, high→`{P1,P2,VRC6_P1,VRC6_P2}`. Tagging Triangle/Saw as `SplitTier::Bass` and squares as `SplitTier::Lead`, then partitioning the active set by tier, reproduces this exactly. Noise/DMC are `Percussion` (never in the active melodic set), matching their current exclusion. So the refactor is behavior-preserving by construction; the regression test asserts it.

## File Structure

**Create:**
- `src/apu/ChannelRegistry.h` — `nessy::` enums (`ChipGroup`, `ChannelKind`, `ChannelRole`, `SplitTier`), `ChannelDesc` struct, and the `constexpr` production channel table `kChannels` (the current 8 channels). The single source of truth for the channel set.
- `src/apu/IVoiceSink.h` — `nessy::IVoiceSink`, the 3-method abstract boundary the allocator calls on the APU (`noteOn`, `noteOff`, `isChannelEnabled`). Decouples the allocator from `NessyAPU`.
- `tests/cpu/MockVoiceSink.h` — test double implementing `IVoiceSink`; records `noteOn`/`noteOff` calls and answers `isChannelEnabled` from a configurable mask.
- `tests/cpu/test_voice_allocator.cpp` — the Catch2 suite (registry sanity + allocation behavior + regression + N>8).

**Modify:**
- `src/apu/VoiceAllocator.h` — registry-driven, N-channel; hold `IVoiceSink*`; `std::vector<Voice>` parallel to the active registry; per-group enable state; drop the dead `setChannelOrder` + `m_channelOrder`.
- `src/apu/VoiceAllocator.cpp` — rewrite allocation against the registry/positions; no longer includes `NessyAPU.h`.
- `src/apu/NessyAPU.h` — `class NessyAPU : public nessy::IVoiceSink` and mark the 3 methods `override` (behavior-neutral; the methods already exist).
- `tests/CMakeLists.txt` — add the new test file + `VoiceAllocator.cpp` to the `nessy_tests` target (no JUCE needed).

**Unchanged:** `NessyAPU.cpp` (audio), `PluginProcessor` (the `setAPU(apu.get())` call still compiles — `NessyAPU*` upcasts to `IVoiceSink*`), all params, all UI.

---

### Task 1: Channel registry + voice-sink interface

**Files:**
- Create: `src/apu/ChannelRegistry.h`
- Create: `src/apu/IVoiceSink.h`
- Create: `tests/cpu/test_voice_allocator.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `src/apu/ChannelRegistry.h`**

```cpp
#pragma once

// Channel registry: the single, data-driven source of truth for every NES
// channel the synth can allocate. Phase C.1 lists the current 2A03 + VRC6
// channels; future expansion chips (MMC5, FME7/5B, FDS, N163, VRC7) are added
// as rows here when their audio lands (C.2+). GPL-3.0.

#include <array>
#include <cstddef>

namespace nessy {

// One bit per expansion chip; Core2A03 is always enabled.
enum class ChipGroup { Core2A03, VRC6, MMC5, FME7, FDS, N163, VRC7, Count };

enum class ChannelKind { Square, Triangle, Saw, Noise, Dpcm, Wavetable, FM };

enum class ChannelRole { Melodic, Percussion };

// Which side of the pitch-split a melodic channel serves (None for percussion).
enum class SplitTier { Lead, Bass, None };

struct ChannelDesc {
  int         id;        // NES channel id understood by NessyAPU (0-based)
  ChipGroup   group;
  ChannelKind kind;
  ChannelRole role;
  SplitTier   splitTier;
};

// Production channel set (C.1 = today's 2A03 + VRC6 layout). Row order matters:
// the allocator iterates these in order, so this order reproduces the legacy
// channel priority {0,1,2,5,6,7}.
inline constexpr std::array<ChannelDesc, 8> kChannels = {{
    {0, ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {1, ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {2, ChipGroup::Core2A03, ChannelKind::Triangle, ChannelRole::Melodic,    SplitTier::Bass},
    {3, ChipGroup::Core2A03, ChannelKind::Noise,    ChannelRole::Percussion, SplitTier::None},
    {4, ChipGroup::Core2A03, ChannelKind::Dpcm,     ChannelRole::Percussion, SplitTier::None},
    {5, ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {6, ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {7, ChipGroup::VRC6,     ChannelKind::Saw,      ChannelRole::Melodic,    SplitTier::Bass},
}};

} // namespace nessy
```

- [ ] **Step 2: Write `src/apu/IVoiceSink.h`**

```cpp
#pragma once

// IVoiceSink: the minimal boundary VoiceAllocator uses to drive sound. NessyAPU
// implements it (it already has these exact methods); tests use a mock. This
// keeps VoiceAllocator free of any JUCE/NSFPlay dependency so it unit-tests in
// isolation. GPL-3.0.

namespace nessy {

class IVoiceSink {
public:
  virtual ~IVoiceSink() = default;

  // Match NessyAPU's existing signatures exactly.
  virtual void noteOn(int channel, int midiNote, float velocity) = 0;
  virtual void noteOff(int channel) = 0;
  virtual bool isChannelEnabled(int channel) const = 0;
};

} // namespace nessy
```

- [ ] **Step 3: Write the registry sanity test in `tests/cpu/test_voice_allocator.cpp`**

```cpp
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
}
```

- [ ] **Step 4: Wire the test file into `tests/CMakeLists.txt`**

Add `cpu/test_voice_allocator.cpp` to the `target_sources(nessy_tests ...)` list (the block starting at line 17), and add `${CMAKE_SOURCE_DIR}/src/apu` is already on the include path (line 41), so `#include "ChannelRegistry.h"` resolves. Concretely, change:

```cmake
target_sources(nessy_tests PRIVATE
    cpu/test_expansion_chips.cpp
    cpu/test_nsf_engine.cpp
    cpu/test_nsf_bridge.cpp
```
to:
```cmake
target_sources(nessy_tests PRIVATE
    cpu/test_expansion_chips.cpp
    cpu/test_nsf_engine.cpp
    cpu/test_nsf_bridge.cpp
    cpu/test_voice_allocator.cpp
```

- [ ] **Step 5: Configure + build + run the registry test**

Run:
```bash
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release -R "channel registry" --output-on-failure
```
Expected: configure picks up the new source; PASS (`1 test passed`). If CMake doesn't regenerate, run `cmake -B build -G "Visual Studio 17 2022"` first.

- [ ] **Step 6: Commit**

```bash
git add src/apu/ChannelRegistry.h src/apu/IVoiceSink.h tests/cpu/test_voice_allocator.cpp tests/CMakeLists.txt
git commit -m "feat(voicealloc): add channel registry + IVoiceSink (Phase C.1)"
```

---

### Task 2: Write the failing allocator behavior tests (TDD red)

These tests target the **new** allocator API (`setAPU(IVoiceSink*)`, `setChannels(span)`, `setGroupEnabled`). They will not compile until Task 3 lands — that is the intended red state, so **do not commit in this task.**

**Files:**
- Create: `tests/cpu/MockVoiceSink.h`
- Modify: `tests/cpu/test_voice_allocator.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write `tests/cpu/MockVoiceSink.h`**

```cpp
#pragma once

// Test double for IVoiceSink: records the sequence of noteOn/noteOff channel
// ids the allocator drives, and answers isChannelEnabled from a mask.

#include "IVoiceSink.h"
#include <set>
#include <vector>

struct MockVoiceSink : public nessy::IVoiceSink {
  struct OnEvent { int channel; int note; float velocity; };

  std::vector<OnEvent> ons;        // every noteOn, in order
  std::vector<int>     offs;       // every noteOff channel, in order
  std::set<int>        disabled;   // channels reported disabled

  void noteOn(int channel, int midiNote, float velocity) override {
    ons.push_back({channel, midiNote, velocity});
  }
  void noteOff(int channel) override { offs.push_back(channel); }
  bool isChannelEnabled(int channel) const override {
    return disabled.find(channel) == disabled.end();
  }

  // Helpers for assertions.
  std::vector<int> onChannels() const {
    std::vector<int> v;
    for (auto& e : ons) v.push_back(e.channel);
    return v;
  }
  int lastOnChannelFor(int note) const {
    for (auto it = ons.rbegin(); it != ons.rend(); ++it)
      if (it->note == note) return it->channel;
    return -1;
  }
  void clear() { ons.clear(); offs.clear(); }
};
```

- [ ] **Step 2: Append the behavior tests to `tests/cpu/test_voice_allocator.cpp`**

Add these includes at the top of the file (below the existing `#include "ChannelRegistry.h"`):
```cpp
#include "VoiceAllocator.h"
#include "MockVoiceSink.h"
#include <array>
```

Then append:
```cpp
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
```

- [ ] **Step 3: Add `VoiceAllocator.cpp` to the test target in `tests/CMakeLists.txt`**

Append to the `target_sources(nessy_tests ...)` source list:
```cmake
    ${CMAKE_SOURCE_DIR}/src/apu/VoiceAllocator.cpp
```

- [ ] **Step 4: Build and confirm it FAILS to compile (red)**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target nessy_tests
```
Expected: **compile error** — `VoiceAllocator` has no `setChannels` / `setGroupEnabled`, `setAPU` takes `NessyAPU*` not `IVoiceSink*`, and `VoiceAllocator.cpp` still `#include "NessyAPU.h"`. This confirms the tests bind to the new API. Do not commit.

---

### Task 3: Refactor VoiceAllocator to registry-driven + IVoiceSink (TDD green)

**Files:**
- Modify: `src/apu/VoiceAllocator.h`
- Modify: `src/apu/VoiceAllocator.cpp`
- Modify: `src/apu/NessyAPU.h:60-65` (inherit `IVoiceSink`, mark overrides)

- [ ] **Step 1: Replace `src/apu/VoiceAllocator.h` with the registry-driven version**

```cpp
#pragma once

// VoiceAllocator: routes MIDI notes to NES channels over a data-driven channel
// registry (see ChannelRegistry.h). N-channel; per-chip-group enable; modes
// Round-Robin / Pitch-Split / Unison. Drives sound through IVoiceSink so it is
// decoupled from NessyAPU (and unit-testable). GPL-3.0.

#include "ChannelRegistry.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nessy { class IVoiceSink; }

class VoiceAllocator {
public:
  enum class Mode { ROUND_ROBIN, PITCH_SPLIT, UNISON };

  VoiceAllocator();

  // The active channel set (defaults to the production registry). Resets voices.
  void setChannels(const nessy::ChannelDesc *channels, std::size_t count);

  void setAPU(nessy::IVoiceSink *apu) { m_apu = apu; }
  void setMode(Mode mode) { m_mode = mode; }
  Mode getMode() const { return m_mode; }

  // Per-chip-group enable. Core2A03 is always enabled.
  void setGroupEnabled(nessy::ChipGroup group, bool enabled);
  bool isGroupEnabled(nessy::ChipGroup group) const;

  // Back-compat shim used by PluginProcessor: VRC6 group toggle.
  void setVRC6Enabled(bool enabled) { setGroupEnabled(nessy::ChipGroup::VRC6, enabled); }
  bool isVRC6Enabled() const { return isGroupEnabled(nessy::ChipGroup::VRC6); }

  void setSplitPoint(int midiNote) { m_splitPoint = midiNote; }
  int getSplitPoint() const { return m_splitPoint; }

  void noteOn(int midiChannel, int noteNumber, float velocity);
  void noteOff(int midiChannel, int noteNumber);
  void allNotesOff();

  void arpNoteOn(int midiNote, float velocity);
  void arpNoteOff();

  // NES channel id currently holding noteNumber, or -1.
  int getChannelForNote(int noteNumber) const;

  // Channel id constants for UI reference (unchanged values).
  static constexpr int PULSE1 = 0;
  static constexpr int PULSE2 = 1;
  static constexpr int TRIANGLE = 2;
  static constexpr int NOISE = 3;
  static constexpr int VRC6_PULSE1 = 5;
  static constexpr int VRC6_PULSE2 = 6;
  static constexpr int VRC6_SAW = 7;

private:
  struct Voice {
    int noteNumber = -1;
    float velocity = 0.0f;
    uint32_t timestamp = 0;
  };

  // Indices below are POSITIONS into m_channels / m_voices, not channel ids.
  bool positionActive(std::size_t pos) const;             // melodic & group enabled
  int findFreePosition() const;                           // first active free
  int findOldestPosition() const;                         // active, lowest timestamp
  int findPositionForPitch(int noteNumber) const;         // tier-partitioned split
  int findFreeInTier(nessy::SplitTier tier) const;
  int findOldestInTier(nessy::SplitTier tier) const;
  void triggerPosition(int pos, int noteNumber, float velocity);

  nessy::IVoiceSink *m_apu = nullptr;
  Mode m_mode = Mode::ROUND_ROBIN;
  int m_splitPoint = 60;

  const nessy::ChannelDesc *m_channels = nullptr;
  std::size_t m_count = 0;
  std::vector<Voice> m_voices;        // parallel to m_channels
  std::array<bool, static_cast<std::size_t>(nessy::ChipGroup::Count)> m_groupEnabled{};
  uint32_t m_timestamp = 0;

  int m_arpLastNote = -1;
};
```

- [ ] **Step 2: Replace `src/apu/VoiceAllocator.cpp` with the registry-driven implementation**

```cpp
// VoiceAllocator: registry-driven MIDI -> NES channel routing. GPL-3.0.

#include "VoiceAllocator.h"
#include "IVoiceSink.h"

using nessy::ChannelDesc;
using nessy::ChannelRole;
using nessy::ChipGroup;
using nessy::SplitTier;

VoiceAllocator::VoiceAllocator() {
  m_groupEnabled[static_cast<std::size_t>(ChipGroup::Core2A03)] = true;  // always on
  setChannels(nessy::kChannels.data(), nessy::kChannels.size());
}

void VoiceAllocator::setChannels(const ChannelDesc *channels, std::size_t count) {
  m_channels = channels;
  m_count = count;
  m_voices.assign(count, Voice{});
  m_timestamp = 0;
}

void VoiceAllocator::setGroupEnabled(ChipGroup group, bool enabled) {
  if (group == ChipGroup::Core2A03) return;  // Core is always enabled
  m_groupEnabled[static_cast<std::size_t>(group)] = enabled;
}

bool VoiceAllocator::isGroupEnabled(ChipGroup group) const {
  return m_groupEnabled[static_cast<std::size_t>(group)];
}

bool VoiceAllocator::positionActive(std::size_t pos) const {
  const ChannelDesc &c = m_channels[pos];
  return c.role == ChannelRole::Melodic && isGroupEnabled(c.group);
}

int VoiceAllocator::findFreePosition() const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (positionActive(i) && m_voices[i].noteNumber < 0)
      return static_cast<int>(i);
  return -1;
}

int VoiceAllocator::findOldestPosition() const {
  int oldest = -1;
  uint32_t oldestTime = 0;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (!positionActive(i)) continue;
    if (oldest < 0 || m_voices[i].timestamp < oldestTime) {
      oldest = static_cast<int>(i);
      oldestTime = m_voices[i].timestamp;
    }
  }
  return oldest;
}

int VoiceAllocator::findFreeInTier(SplitTier tier) const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (positionActive(i) && m_channels[i].splitTier == tier && m_voices[i].noteNumber < 0)
      return static_cast<int>(i);
  return -1;
}

int VoiceAllocator::findOldestInTier(SplitTier tier) const {
  int oldest = -1;
  uint32_t oldestTime = 0;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (!positionActive(i) || m_channels[i].splitTier != tier) continue;
    if (oldest < 0 || m_voices[i].timestamp < oldestTime) {
      oldest = static_cast<int>(i);
      oldestTime = m_voices[i].timestamp;
    }
  }
  return oldest;
}

int VoiceAllocator::findPositionForPitch(int noteNumber) const {
  const SplitTier tier = (noteNumber < m_splitPoint) ? SplitTier::Bass : SplitTier::Lead;
  int pos = findFreeInTier(tier);
  if (pos < 0) pos = findOldestInTier(tier);
  return pos;
}

void VoiceAllocator::triggerPosition(int pos, int noteNumber, float velocity) {
  if (pos < 0 || static_cast<std::size_t>(pos) >= m_count) return;
  if (m_voices[pos].noteNumber >= 0)
    m_apu->noteOff(m_channels[pos].id);
  m_voices[pos].noteNumber = noteNumber;
  m_voices[pos].velocity = velocity;
  m_voices[pos].timestamp = ++m_timestamp;
  m_apu->noteOn(m_channels[pos].id, noteNumber, velocity);
}

void VoiceAllocator::noteOn(int /*midiChannel*/, int noteNumber, float velocity) {
  if (!m_apu) return;

  if (m_mode == Mode::UNISON) {
    for (std::size_t i = 0; i < m_count; ++i) {
      if (!positionActive(i)) continue;
      if (!m_apu->isChannelEnabled(m_channels[i].id)) continue;  // per-channel UI enable
      triggerPosition(static_cast<int>(i), noteNumber, velocity);
    }
    return;
  }

  int pos = -1;
  if (m_mode == Mode::ROUND_ROBIN) {
    for (std::size_t i = 0; i < m_count; ++i)  // re-use a channel already on this note
      if (positionActive(i) && m_voices[i].noteNumber == noteNumber) { pos = static_cast<int>(i); break; }
    if (pos < 0) pos = findFreePosition();
    if (pos < 0) pos = findOldestPosition();
  } else { // PITCH_SPLIT
    pos = findPositionForPitch(noteNumber);
  }
  triggerPosition(pos, noteNumber, velocity);
}

void VoiceAllocator::noteOff(int /*midiChannel*/, int noteNumber) {
  if (!m_apu) return;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (m_voices[i].noteNumber == noteNumber) {
      m_voices[i].noteNumber = -1;
      m_voices[i].velocity = 0.0f;
      m_apu->noteOff(m_channels[i].id);
    }
  }
}

void VoiceAllocator::allNotesOff() {
  for (std::size_t i = 0; i < m_count; ++i) {
    m_voices[i].noteNumber = -1;
    m_voices[i].velocity = 0.0f;
    if (m_apu) m_apu->noteOff(m_channels[i].id);
  }
}

int VoiceAllocator::getChannelForNote(int noteNumber) const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (m_voices[i].noteNumber == noteNumber)
      return m_channels[i].id;
  return -1;
}

void VoiceAllocator::arpNoteOn(int midiNote, float velocity) {
  if (!m_apu) return;
  if (m_arpLastNote >= 0 && m_arpLastNote != midiNote)
    arpNoteOff();
  noteOn(0, midiNote, velocity);
  m_arpLastNote = midiNote;
}

void VoiceAllocator::arpNoteOff() {
  if (!m_apu || m_arpLastNote < 0) return;
  noteOff(0, m_arpLastNote);
  m_arpLastNote = -1;
}
```

- [ ] **Step 3: Make `NessyAPU` implement `IVoiceSink` (behavior-neutral)**

In `src/apu/NessyAPU.h`: add `#include "IVoiceSink.h"` near the other includes, change the class declaration to inherit it, and mark the three methods `override`. The class line becomes:
```cpp
class NessyAPU : public nessy::IVoiceSink {
```
The three method declarations (around lines 60-65) become:
```cpp
  void noteOn(int channel, int midiNote, float velocity) override;
  void noteOff(int channel) override;
  // ... (setChannelEnabled unchanged) ...
  bool isChannelEnabled(int channel) const override { /* existing body unchanged */ }
```
Do not change any method bodies. This only declares that `NessyAPU` satisfies the interface it already matches.

- [ ] **Step 4: Build the tests and confirm GREEN**

Run:
```bash
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release -R "voicealloc" --output-on-failure
```
Expected: compiles; all `[voicealloc]` tests PASS (7 cases). Then run the full suite to confirm no regression:
```bash
ctest --test-dir build -C Release --output-on-failure
```
Expected: all tests pass (prior 15 + the new voicealloc cases).

- [ ] **Step 5: Commit**

```bash
git add src/apu/VoiceAllocator.h src/apu/VoiceAllocator.cpp src/apu/NessyAPU.h tests/cpu/MockVoiceSink.h tests/cpu/test_voice_allocator.cpp tests/CMakeLists.txt
git commit -m "refactor(voicealloc): registry-driven N-channel allocator via IVoiceSink (Phase C.1)"
```

---

### Task 4: Verify the plugin still builds + behaves identically; update docs

`PluginProcessor` calls `voiceAllocator->setAPU(apu.get())` where `apu` is `std::unique_ptr<NessyAPU>`; since `NessyAPU` now derives from `nessy::IVoiceSink`, `apu.get()` (`NessyAPU*`) implicitly converts to `IVoiceSink*` — no call-site change expected. This task confirms the integration and records C.1.

**Files:**
- Modify: `CHANGELOG.md`, `STATE.md`
- (No expected source change in `PluginProcessor`; if the build reveals one, make the minimal fix and note it.)

- [ ] **Step 1: Preserve the current build artifacts**

Discover and back up before rebuilding (mandatory):
```bash
ls build/Nessy_artefacts/Release/Standalone/Nessy.exe \
   build/Nessy_artefacts/Release/VST3/Nessy.vst3 \
   build/tests/Release/nessy_tests.exe
```
Copy all three (the `.vst3` is a bundle dir) into `releases/<YYYY-MM-DD_HHMM>/` and verify the copy before building.

- [ ] **Step 2: Build the full plugin (Release)**

Run:
```bash
cmake --build build --config Release
```
Expected: `Nessy_Standalone` + `Nessy_VST3` link green (the only expected warning is the pre-existing `nes_fds.cpp:368` C4702). If `PluginProcessor` fails to compile, the fix is at most changing the `setAPU` argument site — apply the minimal change and re-build.

- [ ] **Step 3: Manual behavior check (regression by ear) — add to TESTLATER**

The unit suite proves allocation is behavior-identical, but UI/audio can't be auto-verified. Launch `build/Nessy_artefacts/Release/Standalone/Nessy.exe` and confirm: round-robin / pitch-split / unison still play the same channels; VRC6 toggle still extends the pool; arp still works. Add a checked item under TESTLATER.md "Pending" for this (the synth must sound identical to pre-C.1).

- [ ] **Step 4: Update `CHANGELOG.md` and `STATE.md`**

In `CHANGELOG.md` under `## [Unreleased]`, add to `### Changed`:
```markdown
- **VoiceAllocator is now data-driven (Phase C.1)** — channel allocation reads a `ChannelRegistry` (`ChannelDesc` rows: chip group, kind, melodic/percussion role, split tier) instead of hardcoded 8-channel arrays, and runs over an N-channel pool with per-chip-group enables. Behavior is identical for today's 2A03+VRC6 set. The allocator now drives sound through a small `IVoiceSink` interface (implemented by `NessyAPU`), making it unit-testable in isolation; a new Catch2 `VoiceAllocator` suite covers round-robin / pitch-split / unison / steal / group-gating / non-melodic exclusion + an N>8 case. Foundation for adding expansion-chip voices (C.2+).
```
In `STATE.md`: update the Current Phase line to note Phase C.1 (channel/voice infra) landed; bump the test count in the Build Status row (15 + the new voicealloc cases); add `ChannelRegistry` / `IVoiceSink` to the Key Classes table.

- [ ] **Step 5: Commit**

```bash
git add CHANGELOG.md STATE.md TESTLATER.md
# include any minimal PluginProcessor fix if one was needed
git commit -m "docs(voicealloc): record Phase C.1 channel/voice infra refactor"
```

---

## Self-Review

**Spec coverage (§10a):**
- Channel registry (`ChannelDesc { id, group, kind, role, splitTier }`) → Task 1. *Note: the spec wrote `splitCapable`; this plan uses the more expressive `SplitTier {Lead,Bass,None}`, which is required to reproduce the existing low/high split exactly.*
- Generalized `VoiceAllocator` over the active set (enabled groups ∩ melodic), all modes, single split point, non-melodic exclusion → Task 3.
- Behavior preservation for 2A03+VRC6 → guaranteed by registry row order + tiers; asserted by the round-robin / VRC6 / pitch-split / unison tests (Task 2) and the full-plugin build (Task 4).
- Unit tests + regression → Tasks 1-3 (registry sanity + 7 behavior cases incl. the legacy-set assertions and N>8).
- Out of scope (no new params/audio/UI) → respected; the only `NessyAPU` change is behavior-neutral interface inheritance. *Note: the spec said "NessyAPU untouched"; this plan makes one behavior-neutral change (inherit `IVoiceSink`) to enable isolated testing — flagged here as a deliberate, audio-neutral exception. Future-chip rows are NOT added to the registry in C.1 (added with their audio in C.2+); the N>8 generalization is instead proven by the synthetic `kTenLeads` test registry.*

**Placeholder scan:** none — every step has complete code or an exact command.

**Type consistency:** `setChannels(const ChannelDesc*, size_t)`, `setAPU(IVoiceSink*)`, `setGroupEnabled(ChipGroup,bool)`, `setVRC6Enabled(bool)`, `Mode`, and the `nessy::` enum names are used identically across the header, the .cpp, and the tests. `MockVoiceSink` matches the `IVoiceSink` signatures exactly. The allocator works in *positions* internally and converts to channel *ids* (`m_channels[pos].id`) only at the `IVoiceSink` boundary and in `getChannelForNote`.
