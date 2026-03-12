# Architecture

## Overview

Nessy is a JUCE 8 VST3/Standalone plugin that emulates the NES 2A03 APU (and VRC6 expansion chip) with accurate hardware timing. It uses the NSFPlay emulation cores extracted from Dn-FamiTracker (GPL-3.0).

## Component Diagram

```
MIDI Input
    │
    ▼
VoiceAllocator
  ├─ Round-Robin
  ├─ Pitch-Split
  └─ Unison (all enabled melodic channels)
    │
    ▼
NessyAPU
  ├─ NES_APU  (Pulse 1, Pulse 2)         ─► Render() ─┐
  ├─ NES_DMC  (Triangle, Noise, DMC)      ─► Render() ─┤─► Sum ─► Audio Out
  ├─ NES_VRC6 (VRC6 P1, VRC6 P2, Saw)   ─► Render() ─┘
  └─ NessyMemory ($8000–$FFFF DPCM bank)
    │
    ▼ (per-channel raw out[])
ChannelOscilloscope × 7
```

## Data Flow

1. **MIDI → VoiceAllocator**: Maps note-on/off to NES channel indices (0–7).
2. **VoiceAllocator → NessyAPU**: Calls `noteOn(channel, midiNote, velocity)`.
3. **NessyAPU → NSFPlay registers**: Writes 2A03/VRC6 hardware registers (e.g., $4000, $9000).
4. **NSFPlay → Audio**: Each `process()` call clocks APU cores at NTSC rate (1,789,772.7 Hz) and interpolates to host sample rate. `Render()` returns a ~16-bit mixed stereo buffer.
5. **Audio Mix**: `bufferPulse + bufferTND [+ bufferVRC6]` → normalised to float -1..1.
6. **Visualizer tap**: Raw per-channel `out[]` values sampled each clock into ring buffers, read by `ChannelOscilloscope::paint()` at 60 Hz.

## APU Timing

The NES CPU runs at ~1.789 MHz (NTSC). Each audio sample = ~40.6 CPU clocks at 44.1 kHz. `NessyAPU::process()` accumulates fractional clocks and fires `clockAPU()` per sample.

## NSFPlay Core Mapping

| NSFPlay Class | Channels Handled |
|---|---|
| `xgm::NES_APU` | Pulse 1 (`out[0]`), Pulse 2 (`out[1]`) |
| `xgm::NES_DMC` | Triangle (`out[0]`), Noise (`out[1]`), DMC (`out[2]`) |
| `xgm::NES_VRC6` | VRC6 Pulse 1 (`out[0]`), VRC6 Pulse 2 (`out[1]`), VRC6 Saw (`out[2]`) |

## APVTS Parameters

| ID | Type | Target |
|---|---|---|
| `pulse1Enable` / `pulse2Enable` / `triangleEnable` / `noiseEnable` | Bool | `NessyAPU::setChannelEnabled` |
| `pulse1Duty` / `pulse2Duty` | Int (1–4) | `NessyAPU::setPulseDuty` |
| `noiseMode` | Bool | `NessyAPU::setNoiseMode` |
| `vrc6Enable` | Bool | `NessyAPU::setVRC6Enabled` |
| `vrc6Pulse1Duty` / `vrc6Pulse2Duty` | Int (1–8) | `NessyAPU::setVRC6PulseDuty` |
| `voiceMode` | Int (1–3) | `VoiceAllocator::setMode` |
| `splitPoint` | Int (36–84) | `VoiceAllocator::setSplitPoint` |
| `masterVolume` | Float (0–1) | Output gain in `processBlock` |

## Build System

- CMake 3.24+, JUCE 8.0.4, CPM package manager
- Targets: `Nessy` (VST3), `Nessy_Standalone`
- NSFPlay sources compiled directly (no library; source in `src/apu/nsfplay/`)
- Fonts embedded via `juce_add_binary_data` (Inter, Press Start 2P, background PNG)
