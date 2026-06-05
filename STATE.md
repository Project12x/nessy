# State

## Current Phase

**Phase 8 — Hardware Macros (Complete) + Phase 9 Sweep/Portamento + Arpeggiator (landed)**

Macro sequencer (8 presets), standalone arpeggiator, manual hardware pitch sweep (Pulse 1 & 2), and portamento/glide are implemented and parameter-wired. Remaining: full editable macro-grid UI (Phase 8) and MIDI pitch-bend → sweep trigger (Phase 9).

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |

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

**ghostmoon v0.6.0+** — Nessy uses the ghostmoon UI catalog for all controls:

| Component | ghostmoon Type | Usage |
|---|---|---|
| Master volume | `gm::Knob` | Rotary knob with value readout |
| Channel enables (9) | `gm::GmToggleButton` | LED-style toggles |
| Selectors (17) | `gm::ComboSelector` | Duty, voice mode, macros, sweep |
| Sliders (2) | `gm::HSlider` | Split point, portamento speed |
| Oscilloscopes (7) | `gm::Oscilloscope` | Zero-crossing triggered, double-buffered |

## Key Classes

| Class | File | Role |
|---|---|---|
| `NessyAPU` | `src/apu/NessyAPU.cpp` | Wraps NSFPlay cores; note-on/off, mixing, visualizer |
| `NessyMemory` | `src/apu/NessyMemory.h` | Virtual NES address space ($8000–$FFFF) for DMC samples |
| `VoiceAllocator` | `src/apu/VoiceAllocator.cpp` | MIDI → APU channel routing |
| `MacroEngine` | `src/apu/MacroEngine.cpp` | 60Hz frame-rate macro sequencer |
| `NessyAudioProcessor` | `src/PluginProcessor.cpp` | JUCE plugin processor, APVTS |
| `NessyAudioProcessorEditor` | `src/PluginEditor.cpp` | NES-themed UI using ghostmoon catalog |
| `VRC6Exposed` | `src/apu/NessyAPU.cpp` (local) | Subclass of NES_VRC6 exposing protected `out[]` |

## Known Issues / Tech Debt

- C4244 warnings on `uint8_t` assignments from `int` (minor, non-breaking)
