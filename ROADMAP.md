# Roadmap

## Phase 1–6: Core & UI ✅ Complete

- NSFPlay APU core extraction and integration
- Pulse 1/2, Triangle, Noise, VRC6 synthesis
- Voice allocation (Round-Robin, Pitch-Split, Unison)
- APVTS parameter system
- NES hardware-themed UI (Press Start 2P, scanlines, pixel grooves)
- Per-channel oscilloscopes

## Phase 7: Sound & Vision Upgrade 🔶 In Progress

- [x] Per-channel oscilloscopes
- [x] VRC6 expansion UI and synthesis
- [x] Audio output fixed (Render() path)
- [x] Unison full-stack mode
- [ ] **DMC sample mapping** — map MIDI notes to bank of DPCM samples (kick, snare, bass, tom, etc.)
- [ ] **Accurate TND mixing** — non-linear lookup table for Triangle + Noise + DMC

## Phase 8: Authenticity & Polish

- [ ] Hardware length counters / envelope simulation in voice allocation
- [ ] Pitch bend → hardware sweep units (Pulse channels)
- [ ] Portamento / legato mode
- [ ] DAW automation safety audit (APVTS parameter ranges)

## Phase 9: Preset System

- [ ] Factory presets (Mega Man, Contra, Castlevania style patches)
- [ ] User preset save/load (XML via APVTS state)

## Phase 10: Pedalboard3 Validation

- [ ] Load as VST3 in Pedalboard3
- [ ] MIDI routing test
- [ ] CPU profiling pass

## Out of Scope (Authentic Mode)

- ADSR envelope (not NES hardware)
- Per-channel filter (not NES hardware)
- Polyphony beyond 8-voice stack
