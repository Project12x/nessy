# Changelog

All notable changes to Nessy will be documented in this file.

Format: [keepachangelog.com](https://keepachangelog.com/en/1.0.0/)

## [Unreleased]

### Added
- **Hardware macro sequencer (MacroEngine)** — 60 Hz per-channel register sequencer with 8 presets (None, Plain, Vibrato, Vol Decay, Arp Major, Arp Minor, Duty Sweep, Stab) spanning volume/arpeggio/pitch/duty sequence types with loop and release points; per-channel preset selector in the UI
- **Standalone arpeggiator** — held-note arpeggiator (Up/Down/UpDown/Random, 1–4 octaves) ticked at 60 Hz and routed through the active voice-allocation mode; adapted from the Breadbin SID synth
- **Hardware pitch sweep (Pulse 1 & 2)** — manual sweep-unit config exposing $4001/$4005: enable, direction, rate, shift
- **Portamento / glide** — period-interpolated note glide via MacroEngine (enable + speed)
- **ghostmoon UI framework adoption** — all controls now use `gm::Knob`, `gm::GmToggleButton`, `gm::ComboSelector`, `gm::HSlider`, `gm::Oscilloscope`
- Right-click context menus on all controls (Copy/Paste value, Set to Default, MIDI Learn ready)
- Double-click text entry on knob and sliders
- Hover/focus visual states on all controls
- Zero-crossing-triggered oscilloscopes with glow pass (replaced 64-segment blocky scopes)
- melatonin_blur GPU-accelerated shadows/glow via ghostmoon_ui dependency
- C++20 standard (upgraded from C++17)

### Changed

- Removed custom `ChannelOscilloscope` class — replaced by `gm::Oscilloscope` (push-based, double-buffered)
- Oscilloscope data feed moved from pull-in-paint to push-in-timerCallback via `gm::Oscilloscope::process()`
- All APVTS attachments now managed internally by ghostmoon components (eliminated ~40 manual attachment members)

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
