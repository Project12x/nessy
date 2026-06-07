# State

## Current Phase

**Phase 8 — Hardware Macros (Complete) + Phase 9 Sweep/Portamento + Arpeggiator (landed)**

Macro sequencer (8 presets), standalone arpeggiator, manual hardware pitch sweep (Pulse 1 & 2), and portamento/glide are implemented and parameter-wired. Remaining: full editable macro-grid UI (Phase 8) and MIDI pitch-bend → sweep trigger (Phase 9).

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |
| Tests (`nessy_tests`) | ✅ Catch2/CTest (12 tests); km6502 passes Klaus Dormann functional test; MMC5/FDS/N163/VRC7/5B chips render non-silent; NsfEngine parses/inits/renders a synthetic NSF non-silently (Phase B.1) |

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

## Known Issues / Tech Debt

- C4244 warnings on `uint8_t` assignments from `int` (minor, non-breaking)
