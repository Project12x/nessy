# Changelog

All notable changes to Nessy will be documented in this file.

Format: [keepachangelog.com](https://keepachangelog.com/en/1.0.0/)

## [Unreleased]

### Added
- Non-linear NES APU mixing using NESdev resistor-ladder formula (pulse/TND separation)
- DMC factory drum kit: Kick, Snare, Hi-Hat (closed/open), Tom Lo, Tom Hi at $C000–$C500
- MIDI note → DPCM slot mapping following GM drum map (note 36=Kick, 38=Snare, 42=Hi-Hat, etc.)
- DMC channel now triggers — per-note rate and address/length register writes

### Fixed
- Audio silence caused by incorrect mixing (raw `out` values vs `Render()` output)
- Oscilloscope data normalized per channel type (Pulse /15, DMC /127, VRC6 Saw /42)
- VRC6 channels not included in Unison mode due to missing enabled-state propagation
- Duplicate `SetAPU()` call in `NessyAPU::initialize` removed

## [0.2.0] - 2026-02-06

### Added
- NES hardware-themed UI with Press Start 2P font
- NES background tile pattern and scanline overlay
- Power LED with glow effect
- CRT scanline effect across full UI
- Per-channel colour-coded grooves (P1 red, P2 blue, TRI green, NSE orange, VRC6 purple)
- VRC6 expansion section with channel controls

## [0.1.0] - 2026-01-29

### Added
- Initial project scaffold (JUCE 8, CMake, CPM)
- NSFPlay APU cores (nes_apu, nes_dmc, nes_vrc6) extracted from Dn-FamiTracker
- Pulse 1, Pulse 2, Triangle, Noise channel synthesis
- VoiceAllocator: Round-Robin, Pitch-Split, Unison modes
- MIDI keyboard in standalone mode
- APVTS parameter system: channel enables, duty cycles, noise mode, voice mode, split point
- Master volume knob
