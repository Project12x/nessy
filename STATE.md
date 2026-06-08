# State

## Current Phase

**Phase B — NSF Player (Complete, user-verified)**

Nessy loads and plays NSF/NSFe files in a themed floating "NSF PLAYER" window (Front-Loader skin) with metadata, Play/Stop, subsong navigation, and five live per-channel scopes. Built on a restored, Klaus-Dormann-validated 6502 (Phase A.1) and eight vendored expansion chips (Phase A.2); the slim `NsfEngine` (PIMPL) reproduces NSFPlay's bus topology and clocks the ripped 6502's INIT/PLAY. Playback runs in a dedicated processor mode with an RT-safe atomic engine swap.

Prior synth work — Phase 8 hardware macros (8 presets), Phase 9 sweep/portamento, and the standalone arpeggiator — remains landed and parameter-wired. Remaining (optional) NSF items: window polish, clean mode-transition (all-notes-off on switch), and dynamic expansion-chip scopes. The other half of the spec — the five expansion chips as MIDI-playable synth voices (Phase C) plus a multi-chip deck UI (Phase D) — is not yet started.

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |
| Tests (`nessy_tests`) | ✅ Catch2/CTest (15 tests); km6502 passes the Klaus Dormann functional test; MMC5/FDS/N163/VRC7/5B + DMC render non-silent; NsfEngine parses/inits/renders a synthetic NSF; NSF→processor bridge verified |

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
| `NessyAPU` | `src/apu/NessyAPU.cpp` | Wraps NSFPlay cores; note-on/off, mixing, visualizer |
| `NessyMemory` | `src/apu/NessyMemory.h` | Virtual NES address space ($8000–$FFFF) for DMC samples |
| `VoiceAllocator` | `src/apu/VoiceAllocator.cpp` | MIDI → APU channel routing |
| `MacroEngine` | `src/apu/MacroEngine.cpp` | 60Hz frame-rate macro sequencer |
| `NessyAudioProcessor` | `src/PluginProcessor.cpp` | JUCE plugin processor, APVTS |
| `NessyAudioProcessorEditor` | `src/PluginEditor.cpp` | NES Front-Loader UI (juce:: controls + `NessyLookAndFeel`) |
| `NessyLookAndFeel` / `NessyScope` | `src/NessyUI.h` | NES control skin + CRT-glass oscilloscope wrapper |
| `VRC6Exposed` | `src/apu/NessyAPU.cpp` (local) | Subclass of NES_VRC6 exposing protected `out[]` |
| `NsfEngine` | `src/nsf/NsfEngine.cpp` | Slim NSF player engine (PIMPL): NSF/NSFe parse + 6502 INIT/PLAY + chip mix + per-channel scopes |
| `NsfPlayerWindow` / `NsfPlayerView` | `src/NsfPlayerWindow.cpp` | Themed floating NSF-player window (Front-Loader skin): load, metadata, transport, subsong nav, live scopes |

## Known Issues / Tech Debt

- C4244 warnings on `uint8_t` assignments from `int` (minor, non-breaking)
