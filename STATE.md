# State

## Current Phase

**Phase C.2 — MMC5 + Sunsoft 5B Synth Voices (Complete)**

MMC5 (2 pulses, 4 duty cycles) and Sunsoft 5B / FME7 (3 square tones) are now MIDI-playable synth voices through the unified C.1 voice pool at full feature parity: per-chip enable params, MacroEngine (vibrato/decay/arp/duty-sweep/stab), and portamento. Channel count grew 8 → 13. New chip groups default off. 5B pitch verified correct (A4 = 440 Hz). UI controls are Phase D. `NessyAPU` gained `m_mmc5` and `m_fme7` chip instances plus channel-aware `applyMacroVolume`/`applyMacroDuty` helpers.

Prior completed phases — Phase C.1 channel/voice infra, Phase B NSF Player, Phase 8 hardware macros, Phase 9 sweep/portamento, and the standalone arpeggiator — remain landed. Remaining work: multi-chip deck UI (Phase D).

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |
| Tests (`nessy_tests`) | ✅ Catch2/CTest (26 tests); km6502 passes the Klaus Dormann functional test; MMC5/FDS/N163/VRC7/5B + DMC render non-silent; NsfEngine parses/inits/renders a synthetic NSF; NSF→processor bridge verified; VoiceAllocator suite (round-robin / pitch-split / unison / steal / group-gating / N>8 / MMC5-group allocation / 5B-group allocation) |

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
| MMC5 Pulse 1 | ✅ Working | 2A03-style square; 4 duty cycles; 11-bit period; volume/duty/pitch macros + portamento |
| MMC5 Pulse 2 | ✅ Working | 2A03-style square; 4 duty cycles; 11-bit period; volume/duty/pitch macros + portamento |
| 5B Square A | ✅ Working | AY PSG tone; 12-bit period; volume/pitch macros + portamento; pitch verified A4 = 440 Hz |
| 5B Square B | ✅ Working | AY PSG tone; 12-bit period; volume/pitch macros + portamento |
| 5B Square C | ✅ Working | AY PSG tone; 12-bit period; volume/pitch macros + portamento |

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
| `NessyAPU` | `src/apu/NessyAPU.cpp` | Wraps NSFPlay cores (incl. `m_mmc5` MMC5 + `m_fme7` Sunsoft 5B); note-on/off, mixing, visualizer; implements `IVoiceSink`; channel-aware `applyMacroVolume`/`applyMacroDuty` helpers |
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
