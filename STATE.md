# State

## Current Phase

**Phase 8 — Hardware Macros (In Progress)**

## Build Status

| Target | Status |
|---|---|
| Standalone (`.exe`) | ✅ Builds & runs |
| VST3 (`.vst3`) | ✅ Builds |

**Build command (MSBuild direct — CMake generator currently broken):**
```
&"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "build\Nessy_Standalone.vcxproj" /p:Configuration=Release /m /nologo /v:minimal
```

> CMake generator broken: VS2022 instance deregistered from installer. Run VS Installer > Repair to restore `cmake --build`.

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

## Key Classes

| Class | File | Role |
|---|---|---|
| `NessyAPU` | `src/apu/NessyAPU.cpp` | Wraps NSFPlay cores; note-on/off, mixing, visualizer |
| `NessyMemory` | `src/apu/NessyMemory.h` | Virtual NES address space ($8000–$FFFF) for DMC samples |
| `VoiceAllocator` | `src/apu/VoiceAllocator.cpp` | MIDI → APU channel routing |
| `NessyAudioProcessor` | `src/PluginProcessor.cpp` | JUCE plugin processor, APVTS |
| `NessyAudioProcessorEditor` | `src/PluginEditor.cpp` | NES-themed UI, oscilloscopes |
| `ChannelOscilloscope` | `src/PluginEditor.h` | Per-channel waveform visualizer |
| `VRC6Exposed` | `src/apu/NessyAPU.cpp` (local) | Subclass of NES_VRC6 exposing protected `out[]` |

## Known Issues / Tech Debt

- DMC sample mapping not yet implemented (all notes play kick at $C000)
- Non-linear TND mixing not implemented (currently linear sum)
- C4244 warnings on `uint8_t` assignments from `int` (minor, non-breaking)
- CMake generator broken; using MSBuild directly
