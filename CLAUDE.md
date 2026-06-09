# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Nessy is a VST3 & Standalone audio plugin emulating the Ricoh 2A03 APU (NES sound processor) with expansion-chip support (Konami VRC6, MMC5, Sunsoft 5B). A **13-channel** NES synthesizer with hardware-authentic NTSC timing, **and** an NSF/NSFe player that loads and plays NES music files in a themed window. The FDS, Namco 163, and VRC7 cores are also vendored (available to the NSF player; their synth voices are upcoming — Phase C.3/C.4).

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

Automated tests: a Catch2 + CTest suite (`nessy_tests`, ~26 tests) covers the 6502 CPU (Klaus Dormann functional test), the expansion-chip cores, the NSF engine, and the `VoiceAllocator`. Build + run with `cmake --build build --config Release --target nessy_tests` then `ctest --test-dir build -C Release --output-on-failure`. Audio and UI behaviour still require manual verification (Standalone exe / DAW) — tracked in `TESTLATER.md`.

## Architecture

```
MIDI → VoiceAllocator → NessyAPU → AudioProcessor → Editor
```

The processor runs in one of two modes (`playbackMode`): the **Synth** path (MIDI-driven) or the **NSF player** path (CPU-driven). See `ARCHITECTURE.md` for the full design.

**VoiceAllocator** is data-driven: a `ChannelRegistry` (`src/apu/ChannelRegistry.h`, a `constexpr` table of `ChannelDesc` rows) defines the channel set, and the allocator routes MIDI notes over an **N-channel pool** (per-chip-group enables) using one of three modes — Round-Robin, Pitch-Split (low → bass-tier, high → lead-tier), or Unison. It drives sound through the `IVoiceSink` interface that `NessyAPU` implements, so it is decoupled and unit-tested.

**NessyAPU** owns five NSFPlay emulation cores (13 channels total):
- `xgm::NES_APU` — Pulse 1 & 2 (4 duty cycles, 4-bit volume, hardware sweep)
- `xgm::NES_DMC` — Triangle, Noise, DMC (6 factory DPCM drum samples via `NessyMemory`)
- `xgm::NES_VRC6` — VRC6 Pulse 1/2 (8 duty cycles) and Sawtooth (6-bit accumulator)
- `xgm::NES_MMC5` — MMC5 Pulse 1 & 2 (2A03-style, 4 duty cycles)
- `xgm::NES_FME7` — Sunsoft 5B PSG square tones A/B/C

NSFPlay cores clock at 1,789,772.7 Hz (NTSC). `NessyAPU::process()` advances the cores per host sample and **point-samples** each core's `out[]`/`Render()`: the 2A03 uses the NESdev non-linear pulse + TND mix; VRC6/MMC5/5B are summed linearly. **Blip_Buffer is configured but NOT used in the synth output path** — band-limiting is a known open item (see `TESTLATER.md`); do not assume Blip resampling.

**NsfEngine** (`src/nsf/NsfEngine`, PIMPL) is the NSF player: it parses NSF/NSFe and runs a real 6502 (`xgm::NES_CPU`) over the ripped song's INIT/PLAY against the NSFPlay chip bus. Loading is RT-safe (message-thread build → atomic engine swap → timer retire); `src/NsfPlayerWindow` is the themed loader/transport/scope window (opened from the editor's EJECT chip).

**MacroEngine** runs a 60 Hz register sequencer with 8 preset types (Vibrato, Decay, Arpeggio Major/Minor, Duty Sweep, Stab, Custom). One instance per melodic channel. Ticks every ~29,830 CPU clocks (one NES frame).

**Arpeggiator** is a standalone held-note sequencer (Up/Down/UpDown/Random patterns, configurable octave range) that also runs at 60 Hz.

**NessyMemory** implements `xgm::IDevice` to back the virtual $8000–$FFFF NES address space used by the DMC channel for sample lookup.

## Key Files

| File | Role |
|---|---|
| `src/PluginProcessor.h/cpp` | JUCE AudioProcessor, APVTS parameter tree, DSP chain |
| `src/PluginEditor.h/cpp` | NES Front-Loader UI: juce:: controls + themes + scopes |
| `src/NessyUI.h` | `nessy::NessyLookAndFeel` (NES skin) + `nessy::NessyScope` |
| `src/apu/NessyAPU.h/cpp` | Main APU wrapper; 5 chip cores, register writes, point-sampled mixing (13 channels); implements `IVoiceSink` |
| `src/apu/VoiceAllocator.h/cpp` | Registry-driven, N-channel MIDI→channel routing; 3 allocation modes |
| `src/apu/ChannelRegistry.h` | `constexpr` `ChannelDesc` table — single source of truth for the channel set |
| `src/apu/IVoiceSink.h` | 3-method boundary decoupling `VoiceAllocator` from `NessyAPU` |
| `src/apu/MacroEngine.h/cpp` | 60 Hz hardware macro sequencer (all 13 channels) |
| `src/apu/Arpeggiator.h/cpp` | 60 Hz held-note arpeggiator |
| `src/apu/NessyMemory.h` | Virtual NES address space + 6 factory DPCM samples |
| `src/nsf/NsfEngine.h/cpp` | NSF/NSFe player engine (PIMPL): parse + 6502 INIT/PLAY + chip bus + scopes |
| `src/NsfPlayerWindow.h/cpp` | Themed floating NSF-player window (load/metadata/transport/subsong/scopes) |
| `src/apu/nsfplay/` | NSFPlay emulation cores + km6502 CPU — treat as read-only vendor code |
| `src/apu/blip_buffer/` | Blargg Blip_Buffer — vendored (configured but not in the synth output path) |

## UI Framework

Nessy is **GPL-3.0** (NSFPlay forces it), so it must **not** link the proprietary `ghostmoon`. The UI is Nessy's own NES "Front-Loader" skin: custom `paint()` chrome + stock `juce::` controls styled by `nessy::NessyLookAndFeel` (`src/NessyUI.h`), wired to APVTS. Skin-neutral scaffolding comes from **ghostmoon-oss** (sibling repo, MIT): `gm::ui::ScaledEditor`, `gm::ui::Oscilloscope`, and the DSP utils. Do **not** add `gm::` *control widgets* (Knob/ComboSelector/…) — draw primitives are per-skin; Nessy keeps its own paint.

Controls: `juce::Slider` (volume dial, split/glide) + `juce::ComboBox` (duty/macro/arp/sweep) styled by `NessyLookAndFeel`; channel ON/OFF, NSE mode, and sweep enables are painted hit-tested toggles. Scopes: `nessy::NessyScope` wraps `gm::ui::Oscilloscope` (zero-cross triggered, double-buffered, 512-sample), drawn in the NES skin and fed by a 60 Hz editor timer.

## Parameters (APVTS)

Parameters are declared in `PluginProcessor.cpp` and accessed via `getAPVTS()`. Key IDs: channel enables `pulse1Enable`/`pulse2Enable`/`triangleEnable`/`noiseEnable`/`dmcEnable`; expansion enables `vrc6Enable`/`mmc5Enable`/`sunsoft5bEnable`; duties `pulse1Duty`/`pulse2Duty`/`vrc6Pulse1Duty`/`vrc6Pulse2Duty`/`mmc5Pulse1Duty`/`mmc5Pulse2Duty`; `noiseMode`, `voiceMode`, `splitPoint`, `masterVolume`; plus per-channel macro-preset selectors (all 13 channels), sweep configs (enable/direction/rate/shift for Pulse 1 & 2), arpeggiator (enabled/pattern/octaves), and portamento (enabled/speed). (MMC5/5B enables currently have no dedicated UI control — Phase D; reach them via a host's generic parameter view.)

## Dependencies

- **JUCE 8.0.4** — fetched via CPM at configure time (read-only)
- **NSFPlay cores** — vendored in `src/apu/nsfplay/` (read-only)
- **Blip_Buffer** — vendored in `src/apu/blip_buffer/` (read-only)
- **ghostmoon-oss** — sibling repo (folder `../ghostmoongpl`); link `ghostmoon_oss::dsp` + `ghostmoon_oss::core` (MIT). GPL-compatible. Do NOT link the proprietary `ghostmoon`. See `THIRD_PARTY_LICENSES.md`.

C++20 required. Windows + Visual Studio 2022 is the tested platform.

## Current Phase

**Phase C.2 complete** (on branch `feat/nsf-multichip`). Nessy is now a 13-channel synth **and** an NSF player. Done: the earlier synth work (MacroEngine 8-preset sequencer, standalone Arpeggiator, manual Pulse 1/2 sweep, portamento); **Phase A** (real 6502 restored + 5 expansion chips vendored + Catch2 suite); **Phase B** (NSF player — `NsfEngine`, `playbackMode`, themed `NsfPlayerWindow`); **Phase C.1** (data-driven N-channel `VoiceAllocator` — `ChannelRegistry` + `IVoiceSink`); **Phase C.2** (MMC5 + Sunsoft 5B as MIDI-playable synth voices at full macro/portamento parity).

Upcoming: **C.3** (FDS + Namco 163 wavetable voices), **C.4** (VRC7 FM + patch model), **Phase D** (multi-chip synth UI + all-channels scope strip). See `ROADMAP.md`/`STATE.md` for detail and `TESTLATER.md` for pending by-ear checks (including carried-over items: editable macro-grid UI, MIDI pitch-bend → sweep, and the per-block pulse-duty macro stomp on 2A03/MMC5).
