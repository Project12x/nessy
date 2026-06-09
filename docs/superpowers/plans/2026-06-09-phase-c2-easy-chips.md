# Phase C.2 — Easy Chips (MMC5 + Sunsoft 5B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make MMC5 (2 pulses) and Sunsoft 5B / FME7 (3 square tones) MIDI-playable synth voices through the C.1 channel pool, at full feature parity (per-chip enable, MMC5 duty, MacroEngine + portamento), with the 2A03+VRC6 synth unchanged.

**Architecture:** `NessyAPU` gains its own `xgm::NES_MMC5` and `xgm::NES_FME7` instances, integrated exactly like the existing VRC6 (construct → `SetClock`/`SetRate`/`Reset` → `Tick` when the group is enabled → register writes in `noteOn`/`noteOff` → `Render()`+linear-mix in `process()`). The `Channel` enum, all per-channel arrays, and `MacroEngine::NUM_CHANNELS` grow 8 → 13. Macro volume/duty application is refactored into channel-aware `NessyAPU` helpers so it can route per chip. New chip groups default disabled, so the synth is unchanged until enabled via new APVTS params.

**Tech Stack:** C++20, JUCE 8, NSFPlay cores (`xgm::`), CMake + CPM, Catch2 (allocator/registry tests only — audio is verified by ear), Windows/MSVC.

---

## Key facts (verified against the vendored cores)

- **MMC5** (`xgm::NES_MMC5`, `nes_mmc5.h`): 2A03-style squares. `Write($5000/$5004)` = `duty<<6 | 0x30 | vol`; `$5002/$5006` period-lo; `$5003/$5007` period-hi+length; `$5015` = enable bits (0x03 = both squares). Period **identical to 2A03 pulse**: `period = clock/(16·freq) − 1`, **11-bit** (0-2047). Has `TickFrameSequence(clocks)` (call it alongside the DMC's) **and** `Tick(clocks)`. `Render(INT32 b[2])`. `out[3]` is protected (not needed — no scopes in C.2).
- **5B/FME7** (`xgm::NES_FME7`, `nes_fme7.h`): AY/YM PSG. Register access is **indirect**: `Write($C000, reg)` latches the address, `Write($E000, val)` writes data. Regs `0/1`,`2/3`,`4/5` = 12-bit tone period for ch A/B/C; reg `7` = mixer (bits 0-2 = tone **disable** A/B/C, bits 3-5 = noise disable; 1 = disabled); regs `8/9/10` = ch A/B/C amplitude (D3:D0 = 0-15, D4 = envelope mode → keep 0). Period: `period = 1789772 / (32·freq)`, **no −1**, **12-bit** (1-4095). `Tick(clocks)` only (no frame sequence). `Render(INT32 b[2])`.
- **⚠️ 5B rate risk:** `NES_FME7::SetClock`/`SetRate` are effectively ignored — the embedded PSG always runs at `DEFAULT_CLOCK = 1789772 Hz` (internal /8 and /16 dividers). The smoke test confirmed non-silent output, but **pitch correctness through the synth's Tick-per-CPU-clock + Render-per-host-sample path is unverified.** Task 4 has an explicit pitch-check; if A4 ≠ ~440 Hz, escalate (the fix may require adjusting how the FME7 is advanced/rendered, which is out of this plan's happy path).
- **Macro routing:** `MacroEngine::applyMacroTick` writes 2A03 volume/duty via raw `m_apu->writeRegister($4000/$4004/$400C, …)` and delegates pitch/arp to `m_apu->writePitchOffset` / `writeNoteRegisters` (both channel-aware switches). VRC6 channels get **no** volume macro today. For C.2 parity we move volume/duty application into channel-aware `NessyAPU` helpers that route per chip.
- **midiToPeriod bug to fix:** `midiToPeriod` currently treats any `channel >= VRC6_PULSE1` as 12-bit. MMC5 ids (8,9) are `>= VRC6_PULSE1` but are **11-bit** — the clamp must be VRC6-specific.

## File Structure

**Modify:**
- `src/apu/ChannelRegistry.h` — add 5 rows (ids 8-12); groups `MMC5`/`FME7` already in the enum.
- `src/apu/NessyAPU.h` — grow `Channel` enum + `NUM_CHANNELS` 8→13; fix per-channel array initializers; add `m_mmc5`/`m_fme7` + state; declare new setters + channel-aware macro helpers.
- `src/apu/NessyAPU.cpp` — construct/clock/reset/mix the new chips; `noteOn`/`noteOff`/`midiToPeriod`/`writeNoteRegisters`/`writePitchOffset` branches; channel-aware macro helpers; new setters.
- `src/apu/MacroEngine.h` — `NUM_CHANNELS` 8→13.
- `src/apu/MacroEngine.cpp` — route volume/duty through the new `NessyAPU` channel-aware helpers (covers new channels).
- `src/PluginProcessor.cpp` — declare + push `mmc5Enable`, `sunsoft5bEnable`, `mmc5Pulse1Duty`, `mmc5Pulse2Duty`, and 5 macro-preset params.
- `tests/cpu/test_voice_allocator.cpp` — registry rows + a 13-channel allocation test.

**No editor change** (UI is Phase D). New params are reachable via a host's generic parameter view for by-ear testing.

---

### Task 1: Channel set expansion (registry + enum/arrays 8 → 13)

Structural only — adds the 5 channels everywhere they're counted, with no chip audio yet. New groups stay disabled, so allocation and sound are unchanged.

**Files:** `src/apu/ChannelRegistry.h`, `src/apu/NessyAPU.h`, `src/apu/MacroEngine.h`, `tests/cpu/test_voice_allocator.cpp`

- [ ] **Step 1: Add 5 rows to `kChannels` in `src/apu/ChannelRegistry.h`**

Change the array size to 13 and append the rows (after the VRC6 Saw row):
```cpp
inline constexpr std::array<ChannelDesc, 13> kChannels = {{
    {0,  ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {1,  ChipGroup::Core2A03, ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {2,  ChipGroup::Core2A03, ChannelKind::Triangle, ChannelRole::Melodic,    SplitTier::Bass},
    {3,  ChipGroup::Core2A03, ChannelKind::Noise,    ChannelRole::Percussion, SplitTier::None},
    {4,  ChipGroup::Core2A03, ChannelKind::Dpcm,     ChannelRole::Percussion, SplitTier::None},
    {5,  ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {6,  ChipGroup::VRC6,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {7,  ChipGroup::VRC6,     ChannelKind::Saw,      ChannelRole::Melodic,    SplitTier::Bass},
    {8,  ChipGroup::MMC5,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {9,  ChipGroup::MMC5,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {10, ChipGroup::FME7,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {11, ChipGroup::FME7,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
    {12, ChipGroup::FME7,     ChannelKind::Square,   ChannelRole::Melodic,    SplitTier::Lead},
}};
```

- [ ] **Step 2: Grow the `Channel` enum + `NUM_CHANNELS` in `src/apu/NessyAPU.h`**

Replace the enum tail (`NUM_CHANNELS = 8`) so it reads:
```cpp
    // VRC6 expansion channels (5-7)
    VRC6_PULSE1 = 5,
    VRC6_PULSE2 = 6,
    VRC6_SAW = 7,
    // MMC5 expansion (8-9) — 2A03-style squares
    MMC5_PULSE1 = 8,
    MMC5_PULSE2 = 9,
    // Sunsoft 5B / FME7 expansion (10-12) — PSG square tones
    FME7_A = 10,
    FME7_B = 11,
    FME7_C = 12,
    NUM_CHANNELS = 13
  };
```

- [ ] **Step 3: Fix the per-channel array initializers in `src/apu/NessyAPU.h`**

`m_channelEnabled` and `m_currentNote` use 8-element brace initializers; with `NUM_CHANNELS = 13` the trailing 5 would default to `0` (wrong for `m_currentNote`, which must be `-1`). Replace those member declarations:
```cpp
  // Channel state (Core2A03 base on; all expansion groups off by default)
  bool m_channelEnabled[NUM_CHANNELS] = {true,  true,  true,  true,  false,
                                         false, false, false, false, false,
                                         false, false, false};
  int  m_currentNote[NUM_CHANNELS]   = {-1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1};
  float m_velocity[NUM_CHANNELS]     = {0}; // zero-init all 13
```
(The `m_visualizerBuffers[NUM_CHANNELS][...]` and `m_visualizerWritePos[NUM_CHANNELS]` auto-size correctly.)

- [ ] **Step 4: Grow `MacroEngine::NUM_CHANNELS` in `src/apu/MacroEngine.h`**

```cpp
  static constexpr int NUM_CHANNELS = 13;
```
(The `std::array<ChannelState, NUM_CHANNELS> m_channels` and all loop/guard uses scale automatically.)

- [ ] **Step 5: Extend the registry sanity test in `tests/cpu/test_voice_allocator.cpp`**

In the existing `TEST_CASE("channel registry has the expected current layout", ...)`, update counts and add the new rows' checks:
```cpp
  REQUIRE(kChannels.size() == 13);
  REQUIRE(countMelodic() == 11);                    // 6 prior + MMC5x2 + 5Bx3

  // MMC5 group: 2 melodic squares at ids 8,9.
  REQUIRE(kChannels[8].group == ChipGroup::MMC5);
  REQUIRE(kChannels[8].kind  == ChannelKind::Square);
  REQUIRE(kChannels[8].role  == ChannelRole::Melodic);
  REQUIRE(kChannels[9].group == ChipGroup::MMC5);
  // FME7 group: 3 melodic squares at ids 10,11,12.
  REQUIRE(kChannels[10].group == ChipGroup::FME7);
  REQUIRE(kChannels[12].group == ChipGroup::FME7);
  REQUIRE(kChannels[11].splitTier == SplitTier::Lead);
```
(Keep the existing id↔index loop — it now also pins ids 8-12.)

- [ ] **Step 6: Build + run tests**

```bash
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```
Expected: green (registry test reflects 13 rows; allocator tests unaffected — MMC5/FME7 groups default disabled so allocation is unchanged).

- [ ] **Step 7: Commit**

```bash
git add src/apu/ChannelRegistry.h src/apu/NessyAPU.h src/apu/MacroEngine.h tests/cpu/test_voice_allocator.cpp
git commit -m "feat(voicealloc): add MMC5 + 5B channel rows; grow channel count to 13 (Phase C.2)"
```
(Append the `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>` trailer to every commit in this plan.)

---

### Task 2: MMC5 audio + enable/duty params

**Files:** `src/apu/NessyAPU.h`, `src/apu/NessyAPU.cpp`, `src/PluginProcessor.cpp`, `tests/cpu/test_voice_allocator.cpp`

- [ ] **Step 1: Declarations in `src/apu/NessyAPU.h`**

Add to the `xgm` forward-decl block: `class NES_MMC5;`. Add members (near `m_vrc6`):
```cpp
  std::unique_ptr<xgm::NES_MMC5> m_mmc5; // MMC5 expansion (2 squares)
```
Add state (near `m_vrc6PulseDuty`):
```cpp
  bool m_mmc5Enabled = false;
  DutyCycle m_mmc5PulseDuty[2] = {DUTY_50, DUTY_50};
```
Add public methods (near `setVRC6Enabled`):
```cpp
  void setMmc5Enabled(bool enabled);
  void setMmc5PulseDuty(int pulseChannel, DutyCycle duty); // 0-3
```

- [ ] **Step 2: Construct + init + reset in `src/apu/NessyAPU.cpp`**

In the constructor (after the `m_vrc6 = ...` line):
```cpp
  m_mmc5 = std::make_unique<xgm::NES_MMC5>();
```
In `initialize` (after the VRC6 `SetClock`/`SetRate`):
```cpp
  m_mmc5->SetClock(m_clockRate);
  m_mmc5->SetRate(m_sampleRate);
```
In `reset()` (find where `m_vrc6->Reset()` is and add alongside):
```cpp
  m_mmc5->Reset();
```

- [ ] **Step 3: Clock + mix in `src/apu/NessyAPU.cpp`**

In `clockAPU`, after the VRC6 `Tick`:
```cpp
  if (m_mmc5Enabled) {
    m_mmc5->TickFrameSequence(cpuClocks); // MMC5 has its own length/env sequencer
    m_mmc5->Tick(cpuClocks);
  }
```
In `process()`, after the VRC6 mix block (`mixed += bufferVRC6[0] / 65536.0f;`):
```cpp
    if (m_mmc5Enabled) {
      int32_t bufferMMC5[2] = {0};
      m_mmc5->Render(bufferMMC5);
      // MMC5 squares share the 2A03 pulse level; start at the VRC6 scale and
      // tune by ear (see TESTLATER). 65536 is the starting divisor.
      mixed += static_cast<float>(bufferMMC5[0]) / 65536.0f;
    }
```

- [ ] **Step 4: `noteOn`/`noteOff` MMC5 branches in `src/apu/NessyAPU.cpp`**

Add cases to the `noteOn` switch (after `VRC6_SAW`):
```cpp
  case MMC5_PULSE1: {
    m_mmc5->Write(0x5015, 0x03); // enable both MMC5 squares
    uint8_t duty = static_cast<uint8_t>(m_mmc5PulseDuty[0]) << 6;
    m_mmc5->Write(0x5000, duty | 0x30 | volume);
    m_mmc5->Write(0x5002, period & 0xFF);
    m_mmc5->Write(0x5003, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case MMC5_PULSE2: {
    m_mmc5->Write(0x5015, 0x03);
    uint8_t duty = static_cast<uint8_t>(m_mmc5PulseDuty[1]) << 6;
    m_mmc5->Write(0x5004, duty | 0x30 | volume);
    m_mmc5->Write(0x5006, period & 0xFF);
    m_mmc5->Write(0x5007, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
```
Add cases to the `noteOff` switch:
```cpp
  case MMC5_PULSE1: m_mmc5->Write(0x5000, 0x30); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5004, 0x30); break;
```

- [ ] **Step 5: Fix `midiToPeriod` bit-width for MMC5 in `src/apu/NessyAPU.cpp`**

Replace the `maxPeriod` line so only true VRC6 channels are 12-bit (MMC5 is 11-bit):
```cpp
  bool isVrc6 = (channel == VRC6_PULSE1 || channel == VRC6_PULSE2 || channel == VRC6_SAW);
  double maxPeriod = isVrc6 ? 4095.0 : 2047.0;
```
(MMC5 ids 8/9 now correctly clamp to 2047. The `divider` line stays `(channel == TRIANGLE) ? 32.0 : 16.0` — MMC5 uses 16.)

- [ ] **Step 6: Setters in `src/apu/NessyAPU.cpp`** (mirror `setVRC6Enabled`/`setPulseDuty`)

```cpp
void NessyAPU::setMmc5Enabled(bool enabled) {
  m_mmc5Enabled = enabled;
  m_channelEnabled[MMC5_PULSE1] = enabled;
  m_channelEnabled[MMC5_PULSE2] = enabled;
  if (!enabled) {
    m_mmc5->Write(0x5000, 0x30);
    m_mmc5->Write(0x5004, 0x30);
    m_mmc5->Write(0x5015, 0x00);
  }
}

void NessyAPU::setMmc5PulseDuty(int pulseChannel, DutyCycle duty) {
  if (pulseChannel < 0 || pulseChannel > 1) return;
  m_mmc5PulseDuty[pulseChannel] = duty;
  int channel = (pulseChannel == 0) ? MMC5_PULSE1 : MMC5_PULSE2;
  if (m_currentNote[channel] >= 0) {
    uint8_t dutyBits = static_cast<uint8_t>(duty) << 6;
    uint8_t vol = static_cast<uint8_t>(m_velocity[channel] * 15.0f);
    m_mmc5->Write(pulseChannel == 0 ? 0x5000 : 0x5004, dutyBits | 0x30 | vol);
  }
}
```

- [ ] **Step 7: APVTS params in `src/PluginProcessor.cpp`**

In the parameter-layout function (near the `vrc6Enable` / `vrc6Pulse*Duty` block):
```cpp
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("mmc5Enable", 1), "MMC5 Enable", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("mmc5Pulse1Duty", 1), "MMC5 Pulse 1 Duty",
      juce::StringArray{"12.5%", "25%", "50%", "75%"}, 2));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("mmc5Pulse2Duty", 1), "MMC5 Pulse 2 Duty",
      juce::StringArray{"12.5%", "25%", "50%", "75%"}, 2));
```
In `processBlock` (next to the VRC6 enable push):
```cpp
  bool mmc5Enabled = parameters.getRawParameterValue("mmc5Enable")->load() > 0.5f;
  voiceAllocator->setGroupEnabled(nessy::ChipGroup::MMC5, mmc5Enabled);
  apu->setMmc5Enabled(mmc5Enabled);
  apu->setMmc5PulseDuty(0, static_cast<NessyAPU::DutyCycle>(
      static_cast<int>(parameters.getRawParameterValue("mmc5Pulse1Duty")->load())));
  apu->setMmc5PulseDuty(1, static_cast<NessyAPU::DutyCycle>(
      static_cast<int>(parameters.getRawParameterValue("mmc5Pulse2Duty")->load())));
```
(`PluginProcessor.cpp` already sees `nessy::ChipGroup` via `VoiceAllocator.h` → `ChannelRegistry.h`.)

- [ ] **Step 8: Allocator test for the MMC5 group in `tests/cpu/test_voice_allocator.cpp`**

```cpp
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
```

- [ ] **Step 9: Build the full plugin + tests; commit**

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Expected: links green; tests green (incl. the new MMC5 allocation case). Then commit:
```bash
git add src/apu/NessyAPU.h src/apu/NessyAPU.cpp src/PluginProcessor.cpp tests/cpu/test_voice_allocator.cpp
git commit -m "feat(apu): MMC5 (2 pulses) as MIDI synth voices + enable/duty params (Phase C.2)"
```
> **Build preservation:** before the first full-plugin `cmake --build` in this plan, back up the current `build/Nessy_artefacts/Release/{Standalone/Nessy.exe, VST3/Nessy.vst3}` + `build/tests/Release/nessy_tests.exe` to `releases/<YYYY-MM-DD_HHMM>/` and verify the copy.

---

### Task 3: Sunsoft 5B (FME7) audio + enable param

**Files:** `src/apu/NessyAPU.h`, `src/apu/NessyAPU.cpp`, `src/PluginProcessor.cpp`, `tests/cpu/test_voice_allocator.cpp`

- [ ] **Step 1: Declarations in `src/apu/NessyAPU.h`**

Forward-decl block: `class NES_FME7;`. Members:
```cpp
  std::unique_ptr<xgm::NES_FME7> m_fme7; // Sunsoft 5B (3 PSG squares)
```
State:
```cpp
  bool    m_fme7Enabled = false;
  uint8_t m_fme7Mixer   = 0x3F; // all tones+noise disabled (1 = off)
```
Public methods:
```cpp
  void setSunsoft5bEnabled(bool enabled);
```
Private helper:
```cpp
  uint16_t midiToFME7Period(int midiNote) const; // AY PSG period (12-bit)
  void fme7Write(uint8_t reg, uint8_t val);      // latch+data convenience
```

- [ ] **Step 2: Construct/init/reset in `src/apu/NessyAPU.cpp`**

Constructor: `m_fme7 = std::make_unique<xgm::NES_FME7>();`
`initialize`:
```cpp
  m_fme7->SetClock(m_clockRate); // note: NES_FME7 ignores this (runs at fixed DEFAULT_CLOCK)
  m_fme7->SetRate(m_sampleRate);
```
`reset()`: `m_fme7->Reset();` and reset the mixer cache: `m_fme7Mixer = 0x3F;`

- [ ] **Step 3: The `fme7Write` helper + period formula in `src/apu/NessyAPU.cpp`**

```cpp
void NessyAPU::fme7Write(uint8_t reg, uint8_t val) {
  m_fme7->Write(0xC000, reg);   // latch register address
  m_fme7->Write(0xE000, val);   // write data
}

uint16_t NessyAPU::midiToFME7Period(int midiNote) const {
  double freq = FREQ_A4 * std::pow(2.0, (midiNote - MIDI_A4) / 12.0);
  double period = 1789772.0 / (32.0 * freq); // AY PSG, no -1; fixed chip clock
  return static_cast<uint16_t>(juce::jlimit(1.0, 4095.0, period));
}
```

- [ ] **Step 4: Clock + mix in `src/apu/NessyAPU.cpp`**

`clockAPU` (after the MMC5 block):
```cpp
  if (m_fme7Enabled)
    m_fme7->Tick(cpuClocks); // FME7 handles its own internal division
```
`process()` (after the MMC5 mix block):
```cpp
    if (m_fme7Enabled) {
      int32_t bufferFME7[2] = {0};
      m_fme7->Render(bufferFME7);
      // FME7 per-channel peak is ~2700; /8000 is a starting divisor — tune by ear.
      mixed += static_cast<float>(bufferFME7[0]) / 8000.0f;
    }
```

- [ ] **Step 5: `noteOn`/`noteOff` 5B branches in `src/apu/NessyAPU.cpp`**

Helper note: ch A/B/C → tone-period regs (0,1)/(2,3)/(4,5), amplitude regs 8/9/10, mixer tone-disable bits 0/1/2.

Add to `noteOn` switch:
```cpp
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;                 // 0,1,2
    uint16_t p = midiToFME7Period(midiNote);
    fme7Write(idx * 2,     p & 0xFF);           // period low
    fme7Write(idx * 2 + 1, (p >> 8) & 0x0F);    // period high (12-bit)
    m_fme7Mixer &= ~(1 << idx);                 // enable this tone (clear disable bit)
    fme7Write(0x07, m_fme7Mixer);
    fme7Write(0x08 + idx, volume & 0x0F);        // amplitude 0-15, fixed (D4=0)
    break;
  }
```
Add to `noteOff` switch:
```cpp
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    fme7Write(0x08 + idx, 0x00);                // amplitude 0
    m_fme7Mixer |= (1 << idx);                  // disable this tone
    fme7Write(0x07, m_fme7Mixer);
    break;
  }
```

- [ ] **Step 6: `setSunsoft5bEnabled` in `src/apu/NessyAPU.cpp`**

```cpp
void NessyAPU::setSunsoft5bEnabled(bool enabled) {
  m_fme7Enabled = enabled;
  m_channelEnabled[FME7_A] = enabled;
  m_channelEnabled[FME7_B] = enabled;
  m_channelEnabled[FME7_C] = enabled;
  if (!enabled) {
    fme7Write(0x08, 0x00); fme7Write(0x09, 0x00); fme7Write(0x0A, 0x00); // amps 0
    m_fme7Mixer = 0x3F;
    fme7Write(0x07, m_fme7Mixer);
  }
}
```

- [ ] **Step 7: APVTS param + push in `src/PluginProcessor.cpp`**

Layout: `layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("sunsoft5bEnable", 1), "Sunsoft 5B Enable", false));`
processBlock:
```cpp
  bool fme7Enabled = parameters.getRawParameterValue("sunsoft5bEnable")->load() > 0.5f;
  voiceAllocator->setGroupEnabled(nessy::ChipGroup::FME7, fme7Enabled);
  apu->setSunsoft5bEnabled(fme7Enabled);
```

- [ ] **Step 8: Allocator test for the FME7 group in `tests/cpu/test_voice_allocator.cpp`**

```cpp
TEST_CASE("enabling the 5B group adds its three channels to the pool", "[voicealloc]") {
  MockVoiceSink sink;
  VoiceAllocator va;
  va.setChannels(nessy::kChannels.data(), nessy::kChannels.size());
  va.setAPU(&sink);
  va.setGroupEnabled(nessy::ChipGroup::FME7, true);   // Core2A03 (0,1,2) + FME7 (10,11,12)
  va.setMode(VoiceAllocator::Mode::ROUND_ROBIN);

  for (int n = 60; n < 66; ++n) va.noteOn(0, n, 1.0f); // 6 notes -> 0,1,2,10,11,12
  REQUIRE(sink.onChannels() == std::vector<int>{0, 1, 2, 10, 11, 12});
}
```

- [ ] **Step 9: Build + tests + commit**

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
git add src/apu/NessyAPU.h src/apu/NessyAPU.cpp src/PluginProcessor.cpp tests/cpu/test_voice_allocator.cpp
git commit -m "feat(apu): Sunsoft 5B (3 squares) as MIDI synth voices + enable param (Phase C.2)"
```

- [ ] **Step 10: ⚠️ Pitch verification (manual, blocking)**

In a DAW (or the Standalone if a 5B enable control is exposed), enable `sunsoft5bEnable`, route a note to a 5B channel, and play **A4 (MIDI 69)**. Confirm by ear / analyzer the pitch is ~440 Hz.
- If correct: proceed.
- If wrong (octave off, detuned, or aliasing): the cause is the `NES_FME7` fixed-rate behaviour (it ignores `SetRate`). **Report this as a finding** — the fix likely requires rendering the FME7 at its native rate and resampling, or adjusting the Tick cadence; that is beyond this plan's happy path and needs a decision. Do NOT hack the period formula to mask a rate bug.

---

### Task 4: MacroEngine + portamento parity for the new channels

Move volume/duty macro application into channel-aware `NessyAPU` helpers (so they route per chip), extend the pitch/arp routing switches, and add the 5 macro-preset params.

**Files:** `src/apu/NessyAPU.h`, `src/apu/NessyAPU.cpp`, `src/apu/MacroEngine.cpp`, `src/PluginProcessor.cpp`

- [ ] **Step 1: Add channel-aware macro helpers to `src/apu/NessyAPU.h`**

```cpp
  // Channel-aware macro application (routes to the right chip). Used by MacroEngine.
  void applyMacroVolume(int channel, uint8_t volume); // 0-15
  void applyMacroDuty(int channel, uint8_t duty);     // pulse-like channels only
```

- [ ] **Step 2: Implement them in `src/apu/NessyAPU.cpp`** (behaviour-identical to today's 2A03 path, plus the new chips)

```cpp
void NessyAPU::applyMacroVolume(int channel, uint8_t volume) {
  if (volume > 15) volume = 15;
  switch (channel) {
  case PULSE1: writeRegister(0x4000, (getPulseDutyReg(0) << 6) | 0x30 | volume); break;
  case PULSE2: writeRegister(0x4004, (getPulseDutyReg(1) << 6) | 0x30 | volume); break;
  case NOISE:  writeRegister(0x400C, 0x30 | volume); break;
  case VRC6_PULSE1: m_vrc6->Write(0x9000, (static_cast<uint8_t>(m_vrc6PulseDuty[0]) << 4) | volume); break;
  case VRC6_PULSE2: m_vrc6->Write(0xA000, (static_cast<uint8_t>(m_vrc6PulseDuty[1]) << 4) | volume); break;
  case VRC6_SAW:    m_vrc6->Write(0xB000, static_cast<uint8_t>((volume / 15.0f) * 42.0f) & 0x3F); break;
  case MMC5_PULSE1: m_mmc5->Write(0x5000, (static_cast<uint8_t>(m_mmc5PulseDuty[0]) << 6) | 0x30 | volume); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5004, (static_cast<uint8_t>(m_mmc5PulseDuty[1]) << 6) | 0x30 | volume); break;
  case FME7_A: fme7Write(0x08, volume & 0x0F); break;
  case FME7_B: fme7Write(0x09, volume & 0x0F); break;
  case FME7_C: fme7Write(0x0A, volume & 0x0F); break;
  default: break; // Triangle/DMC: no volume register
  }
}

void NessyAPU::applyMacroDuty(int channel, uint8_t duty) {
  switch (channel) {
  case PULSE1: writeRegister(0x4000, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case PULSE2: writeRegister(0x4004, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case MMC5_PULSE1: m_mmc5->Write(0x5000, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5004, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  default: break; // 5B/VRC6 saw/triangle/noise: no duty
  }
}
```
> Note: this gives MMC5/5B **volume** macros (decay/stab), which 2A03 pulses/noise already have. VRC6 channels gain volume-macro support here too (previously absent) — a behaviour change for VRC6, but a strictly additive one consistent with "parity"; flag it in the commit message.

- [ ] **Step 3: Route MacroEngine through the helpers in `src/apu/MacroEngine.cpp`**

In `applyMacroTick`, replace the volume `switch` (cases 0/1/3 writing raw `$4000/$4004/$400C`) with:
```cpp
  if (!m.volume.empty()) {
    int v = advance(ch.volPos, m.volume, ch.released);
    uint8_t vol = static_cast<uint8_t>(std::round(v * ch.velocity));
    m_apu->applyMacroVolume(channel, vol);
  }
```
And replace the duty `switch` (cases 0/1) with:
```cpp
  if (!m.duty.empty()) {
    int d = advance(ch.dutyPos, m.duty, ch.released);
    if (d < 0) d = 0;
    m_apu->applyMacroDuty(channel, static_cast<uint8_t>(d));
  }
```
(Pitch/arpeggio already delegate to `writePitchOffset` / `writeNoteRegisters`, extended next.)

- [ ] **Step 4: Extend the pitch/arp routing switches in `src/apu/NessyAPU.cpp`**

In `writeNoteRegisters`, add cases (before `default:`):
```cpp
  case MMC5_PULSE1:
    m_mmc5->Write(0x5002, period & 0xFF);
    m_mmc5->Write(0x5003, (period >> 8) & 0x07);
    break;
  case MMC5_PULSE2:
    m_mmc5->Write(0x5006, period & 0xFF);
    m_mmc5->Write(0x5007, (period >> 8) & 0x07);
    break;
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    uint16_t p = midiToFME7Period(midiNote); // 5B uses its own period mapping
    fme7Write(idx * 2,     p & 0xFF);
    fme7Write(idx * 2 + 1, (p >> 8) & 0x0F);
    break;
  }
```
In `writePitchOffset`, first fix the clamp to be chip-aware, then add cases. Replace `adjusted = juce::jlimit(0, 0x7FF, adjusted);` with a per-channel max, and for the 5B compute the base from its own formula:
```cpp
  bool isFme7 = (channel >= FME7_A);
  int maxP = (channel == VRC6_PULSE1 || channel == VRC6_PULSE2 || channel == VRC6_SAW || isFme7) ? 0xFFF : 0x7FF;
  uint16_t base = isFme7 ? midiToFME7Period(m_currentNote[channel])
                         : midiToPeriod(m_currentNote[channel], channel);
  int adjusted = static_cast<int>(base) + periodOffset;
  adjusted = juce::jlimit(0, maxP, adjusted);
  uint16_t period = static_cast<uint16_t>(adjusted);
```
(That replaces the existing `base`/`adjusted`/`jlimit`/`period` lines.) Then add switch cases:
```cpp
  case MMC5_PULSE1: m_mmc5->Write(0x5002, period & 0xFF); m_mmc5->Write(0x5003, (period >> 8) & 0x07); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5006, period & 0xFF); m_mmc5->Write(0x5007, (period >> 8) & 0x07); break;
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    fme7Write(idx * 2, period & 0xFF); fme7Write(idx * 2 + 1, (period >> 8) & 0x0F);
    break;
  }
```

- [ ] **Step 5: Macro-preset params for the 5 new channels in `src/PluginProcessor.cpp`**

Extend the `macroIds` / `macroNames` arrays and `macroChannels` mapping (in BOTH the layout-declaration loop and the processBlock push) to 12 entries:
```cpp
  const char* macroIds[] = {
      "macroPulse1", "macroPulse2", "macroTri", "macroNoise",
      "macroVrc6P1", "macroVrc6P2", "macroVrc6Saw",
      "macroMmc5P1", "macroMmc5P2", "macroFme7A", "macroFme7B", "macroFme7C"};
  const char* macroNames[] = {
      "Pulse 1 Macro", "Pulse 2 Macro", "Triangle Macro", "Noise Macro",
      "VRC6 P1 Macro", "VRC6 P2 Macro", "VRC6 Saw Macro",
      "MMC5 P1 Macro", "MMC5 P2 Macro", "5B A Macro", "5B B Macro", "5B C Macro"};
  const int macroChannels[] = {
      NessyAPU::PULSE1, NessyAPU::PULSE2, NessyAPU::TRIANGLE, NessyAPU::NOISE,
      NessyAPU::VRC6_PULSE1, NessyAPU::VRC6_PULSE2, NessyAPU::VRC6_SAW,
      NessyAPU::MMC5_PULSE1, NessyAPU::MMC5_PULSE2,
      NessyAPU::FME7_A, NessyAPU::FME7_B, NessyAPU::FME7_C};
```
Change both loop bounds from `i < 7` to `i < 12`.

- [ ] **Step 6: Build + tests + commit**

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
git add src/apu/NessyAPU.h src/apu/NessyAPU.cpp src/apu/MacroEngine.cpp src/PluginProcessor.cpp
git commit -m "feat(apu): MacroEngine + portamento parity for MMC5/5B channels (Phase C.2)"
```

- [ ] **Step 7: By-ear macro check (manual)** — enable MMC5 and 5B; set Vibrato / Vol Decay / Arp / (MMC5) Duty Sweep / Stab presets on the new channels and confirm they behave like the 2A03 channels; confirm portamento glides. Record results in TESTLATER.

---

### Task 5: Plugin verification + docs

**Files:** `CHANGELOG.md`, `STATE.md`, `TESTLATER.md`

- [ ] **Step 1: Confirm the full plugin is green** (already built in Tasks 2-4) and tests pass:
```bash
ctest --test-dir build -C Release --output-on-failure
```

- [ ] **Step 2: Update `CHANGELOG.md`** — under `## [Unreleased]` → `### Added`:
```markdown
- **MMC5 + Sunsoft 5B synth voices (Phase C.2)** — MMC5 (2 pulses, 4 duties) and Sunsoft 5B (3 square tones) are now MIDI-playable through the unified voice pool, at full parity with the existing channels (per-chip enable params, MacroEngine vibrato/decay/arp/duty-sweep, portamento). New `mmc5Enable`/`sunsoft5bEnable`/`mmc5Pulse*Duty` + 5 macro-preset params; the channel count grew 8 → 13. New chips default off (synth unchanged until enabled); UI controls land in Phase D. 5B uses the AY PSG period mapping.
```
Also add a `### Changed` note: VRC6 channels gained volume-macro support (additive) as a side effect of the channel-aware macro routing.

- [ ] **Step 3: Update `STATE.md`** — Current Phase → Phase C.2 complete; Channels table: add MMC5 P1/P2, 5B A/B/C rows; bump the test count (registry + 2 new allocator cases); note `m_mmc5`/`m_fme7` in the NessyAPU key-class row.

- [ ] **Step 4: Update `TESTLATER.md`** — add a P0 "Phase C.2" block: MMC5 plays 2 pulses with audible duty; 5B plays 3 squares **at correct pitch** (the rate-risk check); macros + portamento on the new channels; mix levels balanced vs 2A03/VRC6; 2A03+VRC6 unchanged; enable toggles work (via DAW generic UI).

- [ ] **Step 5: Commit**
```bash
git add CHANGELOG.md STATE.md TESTLATER.md
git commit -m "docs: record Phase C.2 (MMC5 + Sunsoft 5B synth voices)"
```

---

## Self-Review

**Spec coverage (§10b):**
- Channels/registry +5 → 13 → Task 1. NessyAPU integration (instances, clock, mix, noteOn/off) → Tasks 2 (MMC5) + 3 (5B). Params (enables, MMC5 duty, macro selectors) → Tasks 2/3/4. Macros + portamento full parity → Task 4. Verification (registry + allocator tests; by-ear) → tests in Tasks 1-3, manual in Tasks 2/3/4/5. Out-of-scope (no UI, 5B noise/env excluded, groups default off) → respected. All §10b bullets covered.

**Placeholder scan:** none. The mix scale factors (`/65536`, `/8000`) and the 5B pitch check are explicit empirical/verification steps with starting values + tune/verify instructions, not vague placeholders.

**Type consistency:** `MMC5_PULSE1=8…FME7_C=12` used identically in the enum, registry, noteOn/off, writeNoteRegisters, writePitchOffset, applyMacroVolume/Duty, and the param mapping. `setMmc5Enabled`/`setMmc5PulseDuty`/`setSunsoft5bEnabled`/`applyMacroVolume`/`applyMacroDuty`/`midiToFME7Period`/`fme7Write` are declared in NessyAPU.h (Task 1-4) and defined in NessyAPU.cpp; `MacroEngine` calls `applyMacroVolume`/`applyMacroDuty` (Task 3-4 match). `nessy::ChipGroup::MMC5`/`FME7` match the registry enum.

**Risk register:** (1) 5B pitch via the FME7 fixed-rate path — Task 3 Step 10 gates it. (2) Mix-level balance — starting divisors + by-ear tune. (3) VRC6 volume-macro behaviour change — additive, flagged in the Task 4 commit + CHANGELOG.
