# Changelog

All notable changes to Nessy will be documented in this file.

Format: [keepachangelog.com](https://keepachangelog.com/en/1.0.0/)

## [Unreleased]

### Added
- **Test harness + validated 6502 core (NSF foundation, Phase A.1)** — Catch2 + CTest suite; vendored the public-domain `km6502` CPU core (nsfplay @6af5406) and validated it against the Klaus Dormann 6502 functional test. Foundation for the upcoming NSF player. No change to the synth.
- **Hardware macro sequencer (MacroEngine)** — 60 Hz per-channel register sequencer with 8 presets (None, Plain, Vibrato, Vol Decay, Arp Major, Arp Minor, Duty Sweep, Stab) spanning volume/arpeggio/pitch/duty sequence types with loop and release points; per-channel preset selector in the UI
- **Standalone arpeggiator** — held-note arpeggiator (Up/Down/UpDown/Random, 1–4 octaves) ticked at 60 Hz and routed through the active voice-allocation mode; adapted from the Breadbin SID synth
- **Hardware pitch sweep (Pulse 1 & 2)** — manual sweep-unit config exposing $4001/$4005: enable, direction, rate, shift
- **Portamento / glide** — period-interpolated note glide via MacroEngine (enable + speed)
- **Front-Loader NES UI redesign** — full editor rebuild at 1040×508 (`gm::ui::ScaledEditor`): tiled PCB chassis; louvered header with the NES-controller gamepad cluster (D-pad + SELECT·VOICE / START·ARP pills showing live voice mode + arp pattern·octave, A/B = Split/Porta) and a tactile Volume dial (11-tick ring + pointer); decorative cartridge preset loader (seated "MEGA LEAD" cart, prev/next, EJECT/SAVE, PATCH n/06 — shell only, wires to a real preset system in Phase 11); PATTERN/OCT/SPLIT/GLIDE control rail; channel-bay faceplates (P1 P2 TRI NSE DMC | VRC6 P1/P2/SAW) with CRT-glass oscilloscopes (center-cross graticule + two-layer phosphor bloom); painted channel ON/OFF + NSE mode + sweep toggles; and three persistent themes (NES / Famicom / FDS)
- **Removed proprietary ghostmoon; migrated to ghostmoon-oss (MIT)** — Nessy is GPL-3.0 (NSFPlay) and may not ship proprietary code. Controls are stock `juce::` widgets styled by Nessy's own NES `NessyLookAndFeel`; the skin-neutral DSP utils + `gm::ui::ScaledEditor` + `gm::ui::Oscilloscope` come from the new MIT **ghostmoon-oss**. Added `THIRD_PARTY_LICENSES.md`
- **`dmcEnable` parameter** — DMC channel is now toggleable; base-channel enables (P1/P2/TRI/NSE/DMC) sync to the APU live, not only at load
- Zero-crossing-triggered oscilloscopes (512-sample, double-buffered)
- C++20 standard (upgraded from C++17)

### Changed

- Removed custom `ChannelOscilloscope` — scopes now use the skin-neutral `gm::ui::Oscilloscope` (ghostmoon-oss) drawn by `NessyScope`, fed push-style from the editor's 60 Hz timer
- APVTS control wiring via juce attachments (`SliderAttachment` / `ComboBoxAttachment`) in the editor

### Fixed

- **Hardware macros froze on sustained notes** — `processBlock` re-syncs macro presets every audio block, and `MacroEngine::setPreset` unconditionally cleared the channel's `active` flag (and reallocated the sequence vectors on the audio thread). Held-note macros (vibrato, vol decay, duty sweep, stab, macro-arpeggio) stopped advancing after the note-on block. Root cause: a per-block destructive setter. `setPreset` is now idempotent — it only rebuilds on an actual preset change.
- **Arpeggiator re-sequenced every block** — same root cause: `setArpPattern`/`setArpOctaves` ran `rebuildSequence()` every block, which in Random mode re-shuffled ~90×/sec. The arp setters are now idempotent (no-op when unchanged).

### Previous Unreleased

- Non-linear NES APU mixing using NESdev resistor-ladder formula (pulse/TND separation)
- DMC factory drum kit: Kick, Snare, Hi-Hat (closed/open), Tom Lo, Tom Hi at $C000–$C500
- MIDI note → DPCM slot mapping following GM drum map (note 36=Kick, 38=Snare, 42=Hi-Hat, etc.)
- DMC channel now triggers — per-note rate and address/length register writes

### Fixed (previous)

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
