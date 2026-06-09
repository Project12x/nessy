# Nessy

**NES Multi-Chip Synthesizer + NSF Player — VST3 & Standalone**

Nessy is an authentic NES hardware synthesizer plugin and NSF file player. It emulates four NES sound chips at NTSC clock accuracy and plays back `.nsf`/`.nsfe` game music files using a real 6502 CPU core.

## Synthesizer

### Channels (13 total)

| # | Chip | Channel | Type | Notes |
|---|---|---|---|---|
| 0 | 2A03 | Pulse 1 | Square | 4 duty cycles; 4-bit volume |
| 1 | 2A03 | Pulse 2 | Square | 4 duty cycles; 4-bit volume |
| 2 | 2A03 | Triangle | Triangle | Volume fixed at hardware max |
| 3 | 2A03 | Noise | LFSR noise | Short/long mode; 16 pitch periods |
| 4 | 2A03 | DMC | DPCM sample | 6 factory drums; GM drum-note mapped |
| 5 | VRC6 | Pulse 1 | Square | 8 duty cycles (6.25%–50%) |
| 6 | VRC6 | Pulse 2 | Square | 8 duty cycles |
| 7 | VRC6 | Saw | Sawtooth | Accumulator-based |
| 8 | MMC5 | Pulse 1 | Square | 4 duty cycles; 11-bit period |
| 9 | MMC5 | Pulse 2 | Square | 4 duty cycles; 11-bit period |
| 10 | 5B | Square A | Square | AY PSG; 12-bit period |
| 11 | 5B | Square B | Square | AY PSG |
| 12 | 5B | Square C | Square | AY PSG |

The 2A03 channels are always active. VRC6, MMC5, and Sunsoft 5B are disabled by default — enable via the `vrc6Enable`, `mmc5Enable`, and `sunsoft5bEnable` APVTS parameters (host generic parameter view until Phase D UI lands).

### Voice Modes

- **Round-Robin** — cycles notes through enabled melodic channels
- **Pitch-Split** — low notes → Triangle/Saw (bass-tier), high notes → Pulses (lead-tier)
- **Unison** — all enabled melodic channels play the same note simultaneously

### Hardware Macros

Each channel runs an independent 60 Hz register sequencer with 8 preset types: None, Plain, Vibrato, Vol Decay, Arp Major, Arp Minor, Duty Sweep, Stab. A standalone held-note **Arpeggiator** (Up/Down/UpDown/Random, 1–4 octaves) is also available.

### Pitch & Glide

- **Hardware sweep** (Pulse 1 & 2) — exposes $4001/$4005: period shift, direction, rate, shift amount
- **Portamento** — period-interpolated note glide (works most reliably in Unison mode)

## NSF Player

Click the **EJECT** button on the front panel to open the NSF player window. The player:

- Loads `.nsf` and `.nsfe` files
- Shows title, artist, copyright, and active chip list from the file header
- Play/Stop transport and subsong navigation (◄ SONG n/m ►)
- 5 live per-channel oscilloscopes (P1/P2/TRI/NSE/DMC)
- Supports bankswitched and multi-chip NSFs (VRC6, VRC7, FDS, MMC5, Namco 163, Sunsoft 5B)

Synth mode and NSF mode are mutually exclusive. Switching is RT-safe (no audio-thread allocation or locks).

## Features

- Accurate NTSC APU timing (1,789,772.7 Hz clock)
- NESdev non-linear pulse + TND resistor-ladder mixing formula
- Per-channel enable/disable with live effect on Unison stack
- Per-channel CRT-glass oscilloscope visualizers
- NES "Front-Loader" themed UI with three themes: NES / Famicom / FDS
- APVTS-backed parameters (DAW automation ready)
- MIDI keyboard in Standalone mode
- 26 automated Catch2 tests (CPU, expansion chips, NSF engine, voice allocator)

## Build

See [HOWTO.md](HOWTO.md).

## Philosophy

Nessy stays hardware-authentic. No ADSR envelopes, no filters — just the raw register set of authentic NES chips, exposed through a musician-friendly interface. The NSF player lets you hear what those registers do in real game code.

## License

GPL-3.0. Uses NSFPlay APU cores from [Dn-FamiTracker](https://github.com/Dn-Programming-Core-Management/Dn-FamiTracker) (GPL-3.0) and km6502 CPU core (public domain). See `THIRD_PARTY_LICENSES.md`.
