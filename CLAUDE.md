# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Nessy is a VST3 & Standalone audio plugin emulating the Ricoh 2A03 APU (NES sound processor) with Konami VRC6 expansion support. 8-channel NES synthesizer with hardware-authentic NTSC timing.

## Build Commands

```bash
# First-time configure
cmake -B build -G "Visual Studio 17 2022"

# Build all targets
cmake --build build --config Release

# Build standalone only
cmake --build build --config Release --target Nessy_Standalone
```

**Output paths:**
- Standalone: `build\Nessy_artefacts\Release\Standalone\Nessy.exe`
- VST3: auto-copied to system VST3 folder (`%COMMONFILES%\VST3`)

There are no automated tests. All testing is manual via the Standalone exe or DAW.

## Architecture

```
MIDI → VoiceAllocator → NessyAPU → AudioProcessor → Editor
```

**VoiceAllocator** maps MIDI notes to NES channels using one of three modes: Round-Robin, Pitch-Split (low notes → Triangle/Saw, high notes → Pulses), or Unison (all enabled channels play same note).

**NessyAPU** owns three NSFPlay emulation cores plus Blip_Buffer for band-limited resampling:
- `xgm::NES_APU` — Pulse 1 & 2 (4 duty cycles, 4-bit volume, hardware sweep)
- `xgm::NES_DMC` — Triangle, Noise, DMC (6 factory DPCM drum samples via `NessyMemory`)
- `xgm::NES_VRC6` — VRC6 Pulse 1/2 (8 duty cycles) and Sawtooth (6-bit accumulator)

NSFPlay cores clock at 1,789,772.7 Hz (NTSC). Blip_Buffer resamples to host rate (~40.6 CPU clocks per 44.1 kHz sample). Channels mix in three groups (Pulse, TND, VRC6) with non-linear mixing for the TND group.

**MacroEngine** runs a 60 Hz register sequencer with 8 preset types (Vibrato, Decay, Arpeggio Major/Minor, Duty Sweep, Stab, Custom). One instance per melodic channel. Ticks every ~29,830 CPU clocks (one NES frame).

**Arpeggiator** is a standalone held-note sequencer (Up/Down/UpDown/Random patterns, configurable octave range) that also runs at 60 Hz.

**NessyMemory** implements `xgm::IDevice` to back the virtual $8000–$FFFF NES address space used by the DMC channel for sample lookup.

## Key Files

| File | Role |
|---|---|
| `src/PluginProcessor.h/cpp` | JUCE AudioProcessor, APVTS parameter tree, DSP chain |
| `src/PluginEditor.h/cpp` | ghostmoon UI controls, oscilloscopes, timer callbacks |
| `src/apu/NessyAPU.h/cpp` | Main APU wrapper; register writes, mixing, Blip_Buffer |
| `src/apu/VoiceAllocator.h/cpp` | MIDI→channel demuxing, 3 allocation modes |
| `src/apu/MacroEngine.h/cpp` | 60 Hz hardware macro sequencer |
| `src/apu/Arpeggiator.h/cpp` | 60 Hz held-note arpeggiator |
| `src/apu/NessyMemory.h` | Virtual NES address space + 6 factory DPCM samples |
| `src/apu/nsfplay/` | NSFPlay emulation cores — treat as read-only vendor code |
| `src/apu/blip_buffer/` | Blargg Blip_Buffer — treat as read-only vendor code |

## UI Framework

All UI uses **ghostmoon** (`../ghostmoon/ui`), a sibling repo. Use `gm::` components for any new controls — never raw JUCE components. Check ghostmoon's component catalog before reaching for JUCE directly.

Current controls: `gm::Knob`, `gm::GmToggleButton`, `gm::ComboSelector`, `gm::HSlider`, `gm::Oscilloscope`.

Oscilloscopes are zero-crossing triggered, double-buffered, 512-sample buffers, driven by a 60 Hz timer in the editor.

## Parameters (APVTS)

Parameters are declared in `PluginProcessor.cpp` and accessed via `getAPVTS()`. Key IDs: `pulse1Enable`, `pulse2Enable`, `triangleEnable`, `noiseEnable`, `vrc6Enable`, `pulse1Duty`, `pulse2Duty`, `vrc6Pulse1Duty`, `vrc6Pulse2Duty`, `noiseMode`, `voiceMode`, `splitPoint`, `masterVolume`, plus per-channel macro preset selectors, sweep configs (enable/direction/rate/shift for Pulse 1 & 2), arpeggiator (enabled/pattern/octaves), and portamento (enabled/speed).

## Dependencies

- **JUCE 8.0.4** — fetched via CPM at configure time (read-only)
- **NSFPlay cores** — vendored in `src/apu/nsfplay/` (read-only)
- **Blip_Buffer** — vendored in `src/apu/blip_buffer/` (read-only)
- **ghostmoon_dsp / ghostmoon_ui** — sibling repo at `../ghostmoon/`

C++20 required. Windows + Visual Studio 2022 is the tested platform.

## Current Phase

Phase 8 (Hardware Macros) complete and extended: the MacroEngine 60 Hz sequencer with 8 presets, a standalone Arpeggiator (Up/Down/UpDown/Random, 1–4 octaves), manual hardware pitch sweep (Pulse 1 & 2), and portamento/glide are all implemented and parameter-wired through `PluginProcessor::processBlock`. Remaining: the full editable macro-grid UI (Phase 8) and MIDI pitch-bend → sweep trigger (Phase 9).
