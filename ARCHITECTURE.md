# Architecture

## Overview

Nessy is a JUCE 8 VST3/Standalone plugin with two operating modes that share the same NES chip foundation:

- **Synth mode** — MIDI-driven, 13-channel polyphonic synthesizer (2A03 + VRC6 + MMC5 + Sunsoft 5B)
- **NSF player mode** — loads and plays `.nsf`/`.nsfe` files by running the ripped game code on a real 6502 CPU

Both modes are compiled into the same binary. `PluginProcessor::processBlock` branches on an atomic `playbackMode` flag (0 = Synth, 1 = NSF).

Licensing: GPL-3.0 (forced by the NSFPlay cores). The UI links MIT **ghostmoon-oss** (sibling repo `../ghostmoongpl`) for skin-neutral scaffolding (`ScaledEditor`, `Oscilloscope`, DSP utils). The proprietary `ghostmoon` is **not** linked.

---

## Synth Mode — Data Flow

```
MIDI Input
    │
    ▼
VoiceAllocator  (ChannelRegistry-driven, N-channel, 3 modes)
    │  Round-Robin │ Pitch-Split │ Unison
    │                            (IVoiceSink boundary)
    ▼
NessyAPU  (implements IVoiceSink)
    ├─ xgm::NES_APU   (Pulse 1, Pulse 2)
    ├─ xgm::NES_DMC   (Triangle, Noise, DMC)
    ├─ xgm::NES_VRC6  (VRC6 Pulse 1, Pulse 2, Saw)
    ├─ xgm::NES_MMC5  (MMC5 Pulse 1, Pulse 2)
    ├─ xgm::NES_FME7  (Sunsoft 5B: Square A, B, C)
    └─ NessyMemory    ($8000–$FFFF DPCM bank)
    │
    │  per-sample: advance cores → point-sample out[] / Render()
    │
    ▼
Mix
    ├─ 2A03:   NESdev non-linear pulse + TND resistor-ladder formula
    └─ VRC6 / MMC5 / 5B: linear Render() / 65536 or /8000, summed in
    │
    ▼
Shared DSP tail (ghostmoon-oss)
    DC blocker → SafetyLimiter → master volume (ParamSmoother)
    │
    ▼
Audio Out (stereo; L == R)
    │
    ▼ (per-channel out[] tapped per-sample into ring buffers)
NessyScope × 13  (wraps gm::ui::Oscilloscope; pushed from editor 60 Hz timer)
```

### Mixing

`NessyAPU::process()` advances all chip cores per host sample via `clockAPU()`, then point-samples each core's `out[]` to build the final sample.

- **2A03 (Pulse + TND):** NESdev non-linear resistor-ladder formulas applied to the raw integer `out[]` values. Pulse and TND are summed together first, then mapped to a −1..1 float.
- **VRC6, MMC5, 5B:** each enabled group calls `Render()` (returns a ~16-bit mixed stereo pair); the result is divided by a scale factor and added linearly to the 2A03 mix.

**Blip_Buffer is configured but not used in the output path.** `NessyAPU` instantiates it (for a potential future band-limiting pass), but the current `process()` loop does not pass samples through it. Audio output is therefore point-sampled at the host rate. See TESTLATER P2 for the known aliasing caveat.

### MacroEngine + Arpeggiator

Both run at ~60 Hz inside `clockAPU()`. The macro clock accumulates CPU ticks; every ~29,830 clocks (`1,789,772.7 / 60`) a frame fires, advancing all 13 channel macro sequences and the arpeggiator step.

---

## NSF Player Mode — Data Flow

```
File load (message thread)
    │
    ▼
NsfEngine::load()  (parse NSF/NSFe header, map ROM pages, wire bus)
NsfEngine::init()  (run 6502 INIT routine for the selected song)
    │
    ▼  (atomic pointer publish — no audio-thread allocation)
PluginProcessor::m_pendingNsf  ──► adopted by processBlock()
    │
    ▼
NsfEngine::renderSamples()  (per processBlock: clock 6502 PLAY + chip bus → int16 mono)
    │
    ▼
Mono → stereo bridge → shared DSP tail → Audio Out
    │
    ▼  (per-channel scope ring buffers inside NsfEngine, tapped during render)
NsfPlayerWindow scopes × 5  (P1/P2/TRI/NSE/DMC; pushed from editor 60 Hz timer)
```

### RT-Safe Engine Swap

Loading a new NSF does not allocate on the audio thread:

1. `loadNsf()` (message thread) builds a new `NsfEngine` on the heap and publishes it to `m_pendingNsf` (atomic raw pointer).
2. The next `processBlock()` call adopts the pending pointer into `m_activeNsf` (unique_ptr, audio-thread-owned) and deposits the old engine into `m_retireNsf` (atomic raw pointer).
3. The editor's 60 Hz timer calls `retireOldEngine()` (message thread) to delete what was deposited. The pointee is only ever deleted on the message thread.
4. Scope reads from the message thread go through `m_activeView` (a separate atomic snapshot of the active engine pointer) — never through `m_activeNsf` directly — avoiding a race with the unique_ptr swap.

### NsfEngine PIMPL

`src/nsf/NsfEngine.h` is safe to include alongside JUCE headers. All NSFPlay and km6502 types (including the macro-polluting `nes_cpu.h`) are confined to `NsfEngine.cpp` behind a `struct Impl`. This prevents km6502's short calling-convention macros from leaking into JUCE translation units.

The engine reproduces NSFPlay's bus topology: `NES_CPU` drives a `NES_MEM`/`NES_BANK` backed bus; the chip bus is wired identically to NSFPlay's `NSFPlayer::Reload()`. Per-channel scope ring buffers (5 channels, 512 floats each) are written by the audio thread and read by the message thread under the same benign single-producer/single-consumer pattern as the synth scopes.

---

## Voice Allocation (Synth)

`VoiceAllocator` is data-driven via `ChannelRegistry` (`src/apu/ChannelRegistry.h`): a `constexpr std::array<ChannelDesc, 13>` table where each row holds `{ id, ChipGroup, ChannelKind, ChannelRole(Melodic/Percussion), SplitTier(Lead/Bass/None) }`. The allocator iterates this table rather than hardcoded channel arrays, so adding a new chip requires only a new row in the registry.

The allocator speaks to the APU through `IVoiceSink` (`src/apu/IVoiceSink.h`), a 3-method abstract interface (`noteOn`, `noteOff`, `isChannelEnabled`). This decoupling lets `VoiceAllocator` be unit-tested in isolation with a mock sink.

**Channel pool (13 channels):**

| ID | Chip | Kind | Role | Split |
|---|---|---|---|---|
| 0 | 2A03 | Square (Pulse 1) | Melodic | Lead |
| 1 | 2A03 | Square (Pulse 2) | Melodic | Lead |
| 2 | 2A03 | Triangle | Melodic | Bass |
| 3 | 2A03 | Noise | Percussion | — |
| 4 | 2A03 | DPCM | Percussion | — |
| 5 | VRC6 | Square (P1) | Melodic | Lead |
| 6 | VRC6 | Square (P2) | Melodic | Lead |
| 7 | VRC6 | Saw | Melodic | Bass |
| 8 | MMC5 | Square (P1) | Melodic | Lead |
| 9 | MMC5 | Square (P2) | Melodic | Lead |
| 10 | 5B | Square (A) | Melodic | Lead |
| 11 | 5B | Square (B) | Melodic | Lead |
| 12 | 5B | Square (C) | Melodic | Lead |

VRC6, MMC5, and 5B chip groups default **disabled**; enabling them extends the voice pool. Voice modes:

- **Round-Robin** — cycles enabled melodic channels, steals oldest on overflow
- **Pitch-Split** — low notes → Bass-tier (Triangle/Saw), high notes → Lead-tier (Pulses)
- **Unison** — all enabled melodic channels play the same note simultaneously

---

## APU Timing

NTSC CPU clock: **1,789,772.7 Hz**. `NessyAPU::process()` accumulates fractional clocks per host sample and fires `clockAPU()` with the integer clocks due for that sample (~40.6 CPU clocks per 44.1 kHz sample). `clockAPU` calls `Tick()` on all enabled cores, then fires a macro/arp frame every ~29,830 accumulated clocks.

---

## NSFPlay Core Mapping

| Core | Used by synth | Used by NSF engine | Channels / purpose |
|---|---|---|---|
| `xgm::NES_APU` | Yes | Yes | Pulse 1 (`out[0]`), Pulse 2 (`out[1]`) |
| `xgm::NES_DMC` | Yes | Yes | Triangle (`out[0]`), Noise (`out[1]`), DMC (`out[2]`) |
| `xgm::NES_VRC6` | Yes | Yes | VRC6 Pulse 1/2 (`out[0..1]`), Saw (`out[2]`) |
| `xgm::NES_MMC5` | Yes | Yes | MMC5 Pulse 1/2 |
| `xgm::NES_FME7` | Yes | Yes | Sunsoft 5B: Square A/B/C |
| `xgm::NES_FDS` | No | Yes | FDS wavetable (NSF player only) |
| `xgm::NES_N106` | No | Yes | Namco 163 (NSF player only) |
| `xgm::NES_VRC7` + `emu2413` | No | Yes | VRC7 FM (NSF player only) |
| `xgm::NES_CPU` (km6502) | Idle stub (DMC safety) | Active | 6502 execution (NSF INIT/PLAY) |

---

## Key Classes

| Class | File | Role |
|---|---|---|
| `NessyAudioProcessor` | `src/PluginProcessor.cpp` | JUCE plugin processor; APVTS; `playbackMode` atomic; RT-safe NSF engine swap |
| `NessyAudioProcessorEditor` | `src/PluginEditor.cpp` | NES Front-Loader UI; 60 Hz timer driving scopes + `retireOldEngine()` |
| `NessyAPU` | `src/apu/NessyAPU.cpp` | 5-core synth wrapper; mixes 13 channels; implements `IVoiceSink` |
| `NessyMemory` | `src/apu/NessyMemory.h` | Virtual NES address space ($8000–$FFFF) for synth DMC samples |
| `ChannelRegistry` | `src/apu/ChannelRegistry.h` | `constexpr` table of `ChannelDesc` rows — single source of truth for the channel set |
| `IVoiceSink` | `src/apu/IVoiceSink.h` | `noteOn` / `noteOff` / `isChannelEnabled` interface decoupling allocator from APU |
| `VoiceAllocator` | `src/apu/VoiceAllocator.cpp` | Registry-driven N-channel MIDI→APU routing; 3 voice modes |
| `MacroEngine` | `src/apu/MacroEngine.cpp` | 60 Hz frame-rate register sequencer (8 presets, all 13 channels) |
| `Arpeggiator` | `src/apu/Arpeggiator.cpp` | 60 Hz held-note arpeggiator (Up/Down/UpDown/Random, 1–4 octaves) |
| `NsfEngine` | `src/nsf/NsfEngine.cpp` | PIMPL NSF machine: parse + 6502 INIT/PLAY + chip bus + 5-channel scope buffers |
| `NsfPlayerWindow` / `NsfPlayerView` | `src/NsfPlayerWindow.cpp` | Themed floating NSF-player window: load, metadata, transport, subsong nav, 5 live scopes |
| `NessyLookAndFeel` / `NessyScope` | `src/NessyUI.h` | NES "Front-Loader" control skin + CRT-glass oscilloscope wrapper |
| `VRC6Exposed` | `src/apu/NessyAPU.cpp` (local) | `NES_VRC6` subclass exposing protected `out[]` for the visualizer |

---

## APVTS Parameters

| ID | Type | Target |
|---|---|---|
| `pulse1Enable` / `pulse2Enable` / `triangleEnable` / `noiseEnable` / `dmcEnable` | Bool | `NessyAPU::setChannelEnabled` |
| `vrc6Enable` | Bool | `NessyAPU::setVRC6Enabled` |
| `mmc5Enable` | Bool | `NessyAPU::setMmc5Enabled` |
| `sunsoft5bEnable` | Bool | `NessyAPU::setSunsoft5bEnabled` |
| `pulse1Duty` / `pulse2Duty` | Int (0–3) | `NessyAPU::setPulseDuty` |
| `vrc6Pulse1Duty` / `vrc6Pulse2Duty` | Int (0–7) | `NessyAPU::setVRC6PulseDuty` |
| `mmc5Pulse1Duty` / `mmc5Pulse2Duty` | Int (0–3) | `NessyAPU::setMmc5PulseDuty` |
| `noiseMode` | Bool | `NessyAPU::setNoiseMode` |
| `voiceMode` | Int (1–3) | `VoiceAllocator::setMode` |
| `splitPoint` | Int (36–84) | `VoiceAllocator::setSplitPoint` |
| `masterVolume` | Float (0–1) | Output gain in `processBlock` |
| `macroPulse1`…`macroVrc6Saw` (8), `macroMmc5P1/P2`, `macroFme7A/B/C` (5) | Choice (8 presets) | `NessyAPU::setMacroPreset` → `MacroEngine` |
| `sweep1Enable/Dir/Rate/Shift`, `sweep2…` | Bool / Choice | `NessyAPU::setManualSweepConfig` |
| `portamentoEnable` / `portamentoSpeed` | Bool / Float (1–255) | `NessyAPU::setPortamento` → `MacroEngine` |
| `arpEnable` / `arpPattern` / `arpOctaves` | Bool / Choice / Int (1–4) | `NessyAPU::setArp*` → `Arpeggiator` |
| `uiTheme` | Int (0–2) | `NessyTheme` (NES / Famicom / FDS) |

---

## Build System

- CMake 3.24+, C++20, JUCE 8.0.4 (fetched via CPM)
- **ghostmoon-oss** linked from sibling subdirectory (`../ghostmoongpl`); targets `ghostmoon_oss::dsp` + `ghostmoon_oss::core` (MIT, GPL-compatible). Proprietary `ghostmoon` NOT linked.
- Targets: `Nessy` (VST3), `Nessy_Standalone`, `nessy_tests` (Catch2/CTest)
- NSFPlay + km6502 + emu2413/emu2149 compiled directly from source (no library); source in `src/apu/nsfplay/`. emu2413.c and emu2149.c are C99 and compiled as C.
- Blip_Buffer compiled from source (`src/apu/blip_buffer/`); linked but not currently in the audio output path.
- Fonts + background PNG embedded via `juce_add_binary_data`
