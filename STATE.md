# State

## Current Phase

**Phase C.1 — Channel/Voice Infra (Complete)**

`VoiceAllocator` is now data-driven: channel allocation reads a `ChannelRegistry` (`ChannelDesc` rows per chip group) and routes through an `IVoiceSink` interface implemented by `NessyAPU`. Behavior is identical for today's 2A03+VRC6 set. A new Catch2 `VoiceAllocator` suite (9 test cases) covers all allocation modes, voice steal, group-gating, non-melodic exclusion, and an N>8 generalization. Foundation for adding expansion-chip voices (C.2+).

Prior completed phases — Phase B NSF Player (user-verified), Phase 8 hardware macros (8 presets), Phase 9 sweep/portamento, and the standalone arpeggiator — remain landed. Remaining work: expansion chips as MIDI-playable synth voices (C.2+) and a multi-chip deck UI (Phase D).

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |
| Tests (`nessy_tests`) | ✅ Catch2/CTest (24 tests); km6502 passes the Klaus Dormann functional test; MMC5/FDS/N163/VRC7/5B + DMC render non-silent; NsfEngine parses/inits/renders a synthetic NSF; NSF→processor bridge verified; VoiceAllocator suite (round-robin / pitch-split / unison / steal / group-gating / N>8) |

**Build command:**
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target Nessy_Standalone
```

## Channels

| Channel | Status | Notes |
|---|---|---|
| Pulse 1 | ✅ Working | Duty cycle, volume, MIDI note-to-period |
| Pulse 2 | ✅ Working | Duty cycle, volume, MIDI note-to-period |
| Triangle | ✅ Working | Volume always max (HW limitation) |
| Noise | ✅ Working | Short/long mode, pitch-mapped period |
| DMC | ✅ Working | 6-slot DPCM kit; GM drum note mapping |
| VRC6 Pulse 1 | ✅ Working | 8-level duty cycle |
| VRC6 Pulse 2 | ✅ Working | 8-level duty cycle |
| VRC6 Saw | ✅ Working | Accumulator rate = volume |

## Voice Allocation

| Mode | Status |
|---|---|
| Round-Robin | ✅ |
| Pitch-Split | ✅ |
| Unison (full stack) | ✅ Respects per-channel enable state |

## UI Framework

**Nessy "Front-Loader" NES skin (GPL-clean).** Controls are stock `juce::` widgets styled by Nessy's own `nessy::NessyLookAndFeel` (`src/NessyUI.h`); skin-neutral scaffolding comes from the MIT **ghostmoon-oss** sibling repo. The proprietary `ghostmoon` is **not** linked (Nessy is GPL-3.0). See `THIRD_PARTY_LICENSES.md`.

| Element | Implementation | Usage |
|---|---|---|
| Master volume | `juce::Slider` (rotary) + `NessyLookAndFeel` | tactile dial, 11-tick ring + pointer |
| Channel ON/OFF · NSE mode · P1/P2 sweep enables | painted hit-tested toggles | LED + label, drawn in `paint()` |
| Duty / macro / arp / sweep selectors | `juce::ComboBox` + `NessyLookAndFeel` | NES-inset combos |
| Split / Glide | `juce::Slider` (linear) + `NessyLookAndFeel` | control rail |
| Voice / Arp / Porta / Split | painted gamepad cluster (hit-tested) | header |
| Cartridge preset loader | painted shell (`drawCartridge`) | decorative; wires up Phase 11 |
| Oscilloscopes (8) | `nessy::NessyScope` wrapping `gm::ui::Oscilloscope` | zero-cross triggered, 512-sample, double-buffered |
| Layout / scaling | `gm::ui::ScaledEditor` (ghostmoon-oss, MIT) | fixed 1040×508 artboard, aspect-locked resize |
| Themes | `NessyTheme` (NES / Famicom / FDS) | persisted to APVTS `uiTheme` |

## Key Classes

| Class | File | Role |
|---|---|---|
| `NessyAPU` | `src/apu/NessyAPU.cpp` | Wraps NSFPlay cores; note-on/off, mixing, visualizer; implements `IVoiceSink` |
| `NessyMemory` | `src/apu/NessyMemory.h` | Virtual NES address space ($8000–$FFFF) for DMC samples |
| `ChannelRegistry` | `src/apu/ChannelRegistry.h` | `constexpr` table of `ChannelDesc` rows (chip group, kind, role, split tier); single source of truth for the channel set |
| `IVoiceSink` | `src/apu/IVoiceSink.h` | 3-method abstract boundary (`noteOn`, `noteOff`, `isChannelEnabled`) decoupling `VoiceAllocator` from `NessyAPU` |
| `VoiceAllocator` | `src/apu/VoiceAllocator.cpp` | Registry-driven, N-channel MIDI → APU routing; drives sound via `IVoiceSink` |
| `MacroEngine` | `src/apu/MacroEngine.cpp` | 60Hz frame-rate macro sequencer |
| `NessyAudioProcessor` | `src/PluginProcessor.cpp` | JUCE plugin processor, APVTS |
| `NessyAudioProcessorEditor` | `src/PluginEditor.cpp` | NES Front-Loader UI (juce:: controls + `NessyLookAndFeel`) |
| `NessyLookAndFeel` / `NessyScope` | `src/NessyUI.h` | NES control skin + CRT-glass oscilloscope wrapper |
| `VRC6Exposed` | `src/apu/NessyAPU.cpp` (local) | Subclass of NES_VRC6 exposing protected `out[]` |
| `NsfEngine` | `src/nsf/NsfEngine.cpp` | Slim NSF player engine (PIMPL): NSF/NSFe parse + 6502 INIT/PLAY + chip mix + per-channel scopes |
| `NsfPlayerWindow` / `NsfPlayerView` | `src/NsfPlayerWindow.cpp` | Themed floating NSF-player window (Front-Loader skin): load, metadata, transport, subsong nav, live scopes |

## Known Issues / Tech Debt

- C4244 warnings on `uint8_t` assignments from `int` (minor, non-breaking)
