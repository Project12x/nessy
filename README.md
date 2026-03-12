# Nessy

**NES 2A03 APU Synthesizer — VST3 & Standalone**

Nessy is an authentic NES hardware synthesizer plugin. It emulates the Ricoh 2A03's APU at NTSC clock accuracy, plus the Konami VRC6 expansion chip, giving you 8 channels of pure 8-bit synthesis.

## Channels

| Channel | Type | Max | Notes |
|---|---|---|---|
| Pulse 1 | Square wave | 4-bit | 4 duty cycles (12.5%, 25%, 50%, 75%) |
| Pulse 2 | Square wave | 4-bit | 4 duty cycles |
| Triangle | Triangle wave | — | Volume fixed at hardware max |
| Noise | LFSR noise | 4-bit | Short/long mode, 16 pitch periods |
| DMC | DPCM sample | 7-bit | Factory samples; mapped by MIDI note |
| VRC6 Pulse 1 | Square wave | 4-bit | 8 duty cycles (6.25%–50%) |
| VRC6 Pulse 2 | Square wave | 4-bit | 8 duty cycles |
| VRC6 Saw | Sawtooth | 6-bit | Accumulator-based |

## Voice Modes

- **Round-Robin** — cycles notes through enabled melodic channels
- **Pitch-Split** — low notes → Triangle/Saw, high notes → Pulses
- **Unison** — all enabled melodic channels play the same note simultaneously

## Features

- Accurate NTSC APU timing (1,789,772.7 Hz clock)
- Per-channel enable/disable with live effect on Unison stack
- Per-channel oscilloscope visualizers (8-bit pixelated aesthetic)
- NES hardware-themed UI (Press Start 2P font, scanlines, Power LED)
- APVTS-backed parameters (DAW automation ready)
- MIDI keyboard in Standalone mode

## Philosophy

Nessy stays hardware-authentic. No ADSR envelopes, no filters — just the raw 2A03 register set, exposed through a musician-friendly interface.

## Build

See [HOWTO.md](HOWTO.md).

## License

GPL-3.0. Uses NSFPlay APU cores from [Dn-FamiTracker](https://github.com/Dn-Programming-Core-Management/Dn-FamiTracker) (GPL-3.0).
