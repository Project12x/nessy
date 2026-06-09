# Roadmap

## Phase 1–6: Core Synth & UI ✅ Complete

- NSFPlay APU core extraction and integration
- Pulse 1/2, Triangle, Noise, DMC, VRC6 synthesis (8 channels)
- Voice allocation (Round-Robin, Pitch-Split, Unison full stack)
- APVTS parameter system
- NES "Front-Loader" themed UI (tiled PCB chassis, gamepad cluster, cartridge loader, CRT oscilloscopes)
- Per-channel oscilloscopes; proprietary ghostmoon removed → ghostmoon-oss (MIT)
- Accurate non-linear 2A03 TND mixing (NESdev resistor-ladder)
- DMC factory drum kit + GM drum-note mapping

## Phase 7: Sound & Vision Upgrade ✅ Complete

- [x] Per-channel oscilloscopes with NES aesthetic (zero-cross triggered, 512-sample, double-buffered)
- [x] VRC6 expansion (Pulse 1/2, Sawtooth) fully wired into synth + UI
- [x] Accurate TND non-linear mixing
- [x] Unison full-stack mode (respects per-channel enable)

## Phase 8: Hardware Macros 🔶 Mostly Complete

60 Hz register-write sequencer with 8 preset types per channel. Full editable grid UI still pending.

- [x] Volume sequence — per-frame 4-bit volume (hardware envelope)
- [x] Arpeggio sequence — semitone offset cycling (chord arps)
- [x] Pitch sequence — per-frame period offsets (vibrato, slides)
- [x] Duty cycle sequence — cycling duty widths for timbral animation
- [x] Standalone held-note Arpeggiator (Up/Down/UpDown/Random, 1–4 octaves)
- [ ] Macro editor UI — per-channel sequence editor with loop point (currently preset selectors only)
- [ ] Fix pulse volume/duty macro stomp — `processBlock` re-writes `$4000`/`$4004` every block at ~86 Hz, overriding Vol Decay / Stab / Duty Sweep macros at their 60 Hz tick rate. Same issue affects MMC5 pulses. Fix: make per-block pulse/MMC5-duty sync idempotent (only write on actual duty change). See TESTLATER P0.

## Phase 9: Hardware Sweep Unit 🔶 Partial

- [x] Expose sweep registers ($4001/$4005): period shift, negate flag, divider rate
- [x] Portamento / glide (period-interpolated via MacroEngine)
- [ ] MIDI pitch bend → sweep unit trigger

## Phase A: NSF Foundation ✅ Complete

- [x] Catch2 + CTest test harness added (26 tests, all passing)
- [x] `xgm::NES_CPU` (km6502) restored from stub to full 6502; validated against Klaus Dormann functional test
- [x] Expansion chips vendored: `NES_MMC5`, `NES_FDS`, `NES_N106` (Namco 163), `NES_VRC7` + MIT `emu2413` (OPLL), Sunsoft 5B + MIT `emu2149` (PSG)
- [x] Expansion chips smoke-tested (non-silent output in test suite)

## Phase B: NSF Player ✅ Complete

- [x] **B.1** — `NsfEngine` core: NSF/NSFe parse, 6502 INIT + self-clocking PLAY, chip bus wired from NSFPlay topology; synthetic NSF plays non-silently in tests
- [x] **B.2** — Processor integration: `playbackMode` atomic (Synth vs NSF); RT-safe engine swap (message thread builds, atomic pointer publish, audio thread adopts, message thread retires); NSF audio bridged mono→stereo into shared DSP tail; NsfEngine hidden behind PIMPL to prevent km6502 macro pollution of JUCE TUs
- [x] **B.3** — `NsfPlayerWindow`: themed (Front-Loader skin) floating window; LOAD, metadata (title/artist/copyright/chips), Play/Stop transport, subsong navigation, 5 live per-channel oscilloscopes (P1/P2/TRI/NSE/DMC); opened from editor EJECT button
- [x] **Final review** — RT-safety + lifetime hardening: scope-buffer race fixed (atomic view pointer), stuck-note fix on synth→NSF transition, no audio-thread allocation for oversized blocks, subsong-index clamping, FileChooser UAF guard

## Phase C.1: Channel/Voice Infrastructure ✅ Complete

- [x] `ChannelRegistry` — `constexpr` table of `ChannelDesc` rows replaces hardcoded 8-channel arrays; single source of truth for chip group / kind / role / split tier
- [x] `IVoiceSink` — 3-method abstract interface decoupling `VoiceAllocator` from `NessyAPU`; enables unit-testing the allocator in isolation
- [x] `VoiceAllocator` refactored to N-channel pool with per-chip-group enables; behavior identical for existing 2A03+VRC6 set
- [x] New Catch2 `VoiceAllocator` suite (round-robin / pitch-split / unison / steal / group-gating / N>8 / MMC5 / 5B allocation)

## Phase C.2: Easy Expansion Chips ✅ Complete

- [x] **MMC5** (2 pulses, 4 duty cycles) MIDI-playable at full parity: per-chip enable (`mmc5Enable`), MacroEngine (vibrato/decay/arp/duty-sweep/stab), portamento, APVTS params (`mmc5Pulse1Duty`, `mmc5Pulse2Duty`, `macroMmc5P1/P2`)
- [x] **Sunsoft 5B / FME7** (3 AY PSG square tones) MIDI-playable at full parity: `sunsoft5bEnable`, MacroEngine, portamento, APVTS params (`macroFme7A/B/C`)
- [x] Channel count grew 8 → 13; new chip groups default off
- [x] Channel-aware `applyMacroVolume` / `applyMacroDuty` helpers added; VRC6 Pulse 1/2/Saw gain volume-macro support as a side effect
- [x] `midiToPeriod` now chip-aware; VRC6 Saw pitch clamp widened to 12-bit

**Phase C.2 is complete. Audio + UI by-ear verification is pending — see TESTLATER.**

## Phase C.3: Wavetable Chips (Upcoming)

- [ ] **FDS** (Famicom Disk System) — 64-sample wavetable oscillator; waveform editor UI; wire `NES_FDS` into the synth voice pool
- [ ] **Namco 163 (N163)** — up to 8 simultaneous wavetable channels sharing a wave RAM bank; wire `NES_N106` into the pool with configurable channel count

## Phase C.4: FM Chip (Upcoming)

- [ ] **VRC7** — 6-operator FM (subset of OPL2) via `NES_VRC7` + `emu2413`; patch model (15 ROM patches + 1 user); FM operator parameter editor in the UI

## Phase D: Multi-Chip Synth UI (Upcoming)

The chip infrastructure for 13 channels (+ FDS/N163/VRC7 in C.3–C.4) is in place, but the synth editor still shows only the original 8-channel bay. Phase D builds the full multi-chip deck UI.

- [ ] Expand the channel bay to show all active chips (MMC5, 5B, FDS, N163, VRC7 sections alongside 2A03 + VRC6)
- [ ] Per-chip enable buttons in the UI (currently only reachable via host generic parameter view)
- [ ] All-channels oscilloscope strip (one scope per active channel, not just the base 8)
- [ ] NSF player scopes: expand beyond 5 fixed 2A03 scopes to show expansion-chip channels when a multi-chip NSF is loaded

## Phase 10: DMC Sample Library

- [ ] Convert authentic NES drum kit samples to DPCM (Furnace/FamiTracker pipeline)
- [ ] MIDI note → sample address/length mapping (General MIDI drum map compatible)
- [ ] Rate register exposed per-note for pitch variation

## Phase 11: Presets & Polish

The Front-Loader **cartridge loader UI shell is in place** (decorative: seated cart, prev/next, EJECT/SAVE, PATCH n/06) — wiring to a real backend is pending.

- [ ] Factory presets (Mega Man lead, Contra bass, Castlevania arp, Duck Tales melody)
- [ ] User preset save/load (JSON, A/B comparison, dirty detection) — GPL-clean backend only
- [ ] Wire cartridge UI (prev/next / EJECT / SAVE / PATCH n/06) to the preset backend
- [ ] DAW automation safety audit

## Phase 12: Pedalboard3 Validation

- [ ] Load as VST3 in Pedalboard3
- [ ] MIDI routing test (note-on/off, pitch bend, CC)
- [ ] CPU profiling pass (target: <5% at 44.1 kHz stereo)

## Competitive Gap Summary

| Feature | Status | Phase |
|---|---|---|
| 2A03 pulse + TND synthesis | ✅ | Done |
| VRC6 expansion (P1/P2/Saw) | ✅ | Done |
| MMC5 expansion (P1/P2) | ✅ | C.2 |
| Sunsoft 5B (A/B/C squares) | ✅ | C.2 |
| FDS wavetable | ⬜ | C.3 |
| Namco 163 wavetable | ⬜ | C.3 |
| VRC7 FM | ⬜ | C.4 |
| NSF/NSFe file player | ✅ | B |
| Hardware macros (8 presets) | 🔶 Pulse stomp unresolved | 8 |
| Standalone arpeggiator | ✅ | 8 |
| Hardware pitch sweep (Pulse 1/2) | 🔶 Manual config; no pitch-bend trigger | 9 |
| Portamento / glide | ✅ (Unison mode; limited in RR/Pitch-Split) | 9 |
| 13-channel voice pool | ✅ | C.1/C.2 |
| Multi-chip synth UI | ⬜ | D |
| Quality DMC sample library | 🔶 6 factory samples only | 10 |
| Presets | ⬜ | 11 |

## Out of Scope (Authentic Mode)

- ADSR envelope (not NES hardware)
- Per-channel filter (not NES hardware)
- Polyphony beyond 13-voice stack in Unison
