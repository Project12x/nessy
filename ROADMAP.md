# Roadmap

## Phase 1–6: Core & UI ✅ Complete

- NSFPlay APU core extraction and integration
- Pulse 1/2, Triangle, Noise, DMC, VRC6 synthesis
- Voice allocation (Round-Robin, Pitch-Split, Unison full stack)
- APVTS parameter system
- NES hardware-themed UI (Press Start 2P, scanlines, pixel grooves)
- Per-channel oscilloscopes

## Phase 7: Sound & Vision Upgrade ✅ Complete

- [x] Per-channel oscilloscopes with 8-bit aesthetic
- [x] VRC6 expansion UI and synthesis (Pulse 1, Pulse 2, Sawtooth)
- [x] Audio output fixed (Render() path)
- [x] Unison full-stack mode (respects per-channel enable)
- [x] **DMC sample mapping** — Kick, Snare, Hi-Hat ×2, Tom ×2 with GM drum map
- [x] **Accurate TND mixing** — non-linear resistor-ladder formula (NESdev)

## Phase 8: Hardware Macros 🔶 In Progress

These are the core features behind every recognizable NES sound. All are implemented as frame-rate (60Hz) register write sequences — authentic hardware, not software effects.

- [ ] **Volume sequence** — per-frame 4-bit volume values (hardware envelope). Pulse and Noise only. Triangle ignores volume register.
- [ ] **Arpeggio sequence** — rapid semitone offset cycling (e.g., 0/4/7 → chord strum). Most iconic NES lead technique.
- [ ] **Pitch sequence** — per-frame period offsets for vibrato, slides, and pitch bends. Fine-grained, authentic.
- [ ] **Duty cycle sequence** — cycling through duty widths per frame for timbral animation (Pulse channels only).
- [ ] **Macro editor UI** — per-channel sequence editor with loop point, showing up to 16 frames

## Phase 9: Hardware Sweep Unit (Authenticity Gap #2)

The 2A03 has a built-in hardware pitch sweep on Pulse 1 and Pulse 2. This creates the characteristic bass drops, risers, and glide effects in NES music.

- [ ] Expose sweep registers ($4001, $4005): period shift, negate flag, divider rate
- [ ] MIDI Pitch Bend → sweep unit trigger
- [ ] Portamento/glide mode using sweep units

## Phase 10: DMC Sample Library

- [ ] Convert authentic NES drum kit samples to DPCM (using Furnace/FamiTracker pipeline)
- [ ] Kick, Snare, Hi-Hat (closed/open), Tom, Bass, Cymbal
- [ ] MIDI note → sample address/length mapping (General MIDI drum map compatible)
- [ ] Rate register exposed per-note for pitch variation

## Phase 11: Presets & Polish

- [ ] Factory presets: Mega Man lead, Contra bass, Castlevania arp, Duck Tales melody
- [ ] User preset save/load (APVTS XML)
- [ ] DAW automation safety audit (parameter ranges, thread safety)
- [ ] Accurate non-linear TND mixing table

## Phase 12: Pedalboard3 Validation

- [ ] Load as VST3 in Pedalboard3
- [ ] MIDI routing test (note-on/off, pitch bend, CC)
- [ ] CPU profiling pass (target: <5% at 44.1kHz stereo)

## Competitive Gap Summary

| Feature | Status | Phase |
|---|---|---|
| Hardware sweep unit (pitch glide) | ❌ | 9 |
| Volume sequences (frame-rate envelopes) | ❌ | 8 |
| Arpeggio sequences | ❌ | 8 |
| Pitch sequences (vibrato, slides) | ❌ | 8 |
| Duty cycle sequences | ❌ | 8 |
| Portamento / glide | ❌ | 9 |
| Quality DMC sample library | 🔶 Kick only | 10 |
| All 8 channels working | ✅ | Done |
| Unison stacking | ✅ | Done |
| VRC6 expansion | ✅ | Done |

## Out of Scope (Authentic Mode)

- ADSR envelope (not NES hardware)
- Per-channel filter (not NES hardware)
- Polyphony beyond 8-voice stack

