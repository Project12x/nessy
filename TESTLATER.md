# TESTLATER — Manual Test Backlog

Nessy has 26 automated Catch2 tests (CPU, expansion chips, NSF engine, voice
allocator), but **audio and UI behaviour can't be auto-verified** — those are
checked by hand. This doc is the running list of what a human still needs to
check. Tick a box when verified; annotate `FAIL —` inline if it breaks.

**Last updated:** 2026-06-09 (Phase C.2 MMC5 + Sunsoft 5B synth voices).
Prior: 2026-06-09 (Phase C.1 channel/voice infra refactor); 2026-06-08 (NSF player Phase B + final-review fixes); 2026-06-05 (arp/sweep/portamento).

**How to test:** launch the Standalone
(`build\Nessy_artefacts\Release\Standalone\Nessy.exe`) or load the VST3 in a DAW.
Play via the on-screen keyboard or external MIDI. For audio items, *listen*; for
UI items, *interact* (click, right-click, double-click, drag, resize).

**Priority:**
`P0` recently changed / unverified — check before trusting.
`P1` core behavior & regression surface.
`P2` deeper checks, edge cases, known-issue follow-ups.

---

## P0 — Recent changes, unverified

### Phase C.2 — MMC5 + Sunsoft 5B synth voices (built, 26/26 tests green)

New chips default off. Enable via the host's generic parameter view (`mmc5Enable`, `sunsoft5bEnable`). UI controls are Phase D.

- [ ] **MMC5 — 2 pulses audible.** Enable MMC5, play a chromatic run on each of the 2 MMC5 channels. Confirm both produce a clear square-wave tone. Cycle through all 4 duty settings (`mmc5Pulse1Duty`, `mmc5Pulse2Duty`) and confirm the timbre changes audibly (12.5% → thin, 25% → slightly fuller, 50% → hollow, 75% → same as 25% by hardware symmetry).
- [ ] **5B — 3 squares at correct pitch.** Enable `sunsoft5bEnable`. Play A4 (MIDI 69) on each of the 3 5B channels (A, B, C) and confirm by ear / tuner analyzer that the pitch is ~440 Hz. (Empirically expected to pass per code review — the `NES_FME7` fixed-clock behaviour is fully compensated — but confirm by ear.) Confirm all 3 channels sound simultaneously when 3 notes are held.
- [ ] **Macros on MMC5 channels.** Set `macroMmc5P1` / `macroMmc5P2` to each of: Vibrato (pitch wobbles on a held note), Vol Decay (fades from full to zero), Duty Sweep (duty cycles through values — audible timbre change over time), Stab (short attack burst). Confirm each behaves like the equivalent 2A03 Pulse preset.
- [ ] **Macros on 5B channels.** Set `macroFme7A/B/C` to: Vibrato (pitch wobble), Vol Decay (fade), Stab. Note that Duty Sweep is a no-op on 5B (no duty register) — confirm it does not crash or produce artifacts.
- [ ] **Portamento on MMC5 and 5B.** Enable portamento and play two successive notes on an MMC5 channel and on a 5B channel. Confirm pitch glides between them (same caveat as existing channels: glide works most reliably in Unison mode).
- [ ] **Mix levels vs 2A03/VRC6.** Enable MMC5 and 5B alongside 2A03. Play the same note across chip groups and compare loudness. The `/65536` MMC5 and `/8000` 5B divisors are starting values — tune if one group sounds significantly louder or quieter. Note actual observed balance here for future calibration.
- [ ] **2A03 + VRC6 channels unchanged.** With only the base channels enabled (MMC5 and 5B off), confirm round-robin / pitch-split / unison behave identically to pre-C.2. This is a regression check only.
- [ ] **Enable toggles.** Flip `mmc5Enable` and `sunsoft5bEnable` on/off via the host parameter view while notes are playing. Confirm: toggling off silences the group immediately; toggling back on resumes normal voice allocation; no stuck notes.

### Phase C.1 — Channel/voice infra refactor (built, 24/24 tests green)

The unit suite proves allocation is behavior-identical, but sound and UI cannot be auto-verified.

- [ ] **By-ear regression (Phase C.1).** Launch the Standalone and confirm the synth sounds and behaves identical to pre-C.1: round-robin / pitch-split / unison play the same channels as before; VRC6 toggle still extends the voice pool from 3 to 6; the arpeggiator still works end-to-end. No new audio behavior is expected — this is a regression check only.



### NSF player — Phase-B review fixes (committed `a84919c`, pending by-ear)

Five fixes from the final Phase-B review. They compile clean and pass CTest
(24/24), but touch audio/UI behaviour so they need a by-hand pass. Files
`PluginProcessor.h/.cpp`, `NsfPlayerWindow.cpp`.
Pre-fix build backed up at `releases/2026-06-08_1032/`.

- [ ] **Stuck-note fix — by ear (this one changes audible behaviour).** Hold synth
  notes, then open the NSF window (EJECT) + LOAD an NSF (or click the cartridge
  body to switch to NSF mode). Held synth notes should **cut cleanly** (no hung
  drone); back in synth, notes play **fresh** with no leftover stuck tone.
- [ ] **Subsong clamp (I2).** The ◄ / ► SONG arrows stop at the first/last song —
  they must NOT wrap to a garbage track.
- [ ] **Load-while-scopes-running (C2 data race).** With the window open and scopes
  moving, LOAD a second NSF → smooth swap, no flicker, no crash.
- [ ] **FileChooser teardown (UAF) — edge case.** In a DAW, open the window's LOAD
  dialog then close the plugin window while it's still open → no crash.
  (SafePointer-guarded; skip if hard to stage.)

### ⚠️ Known risk found in code review (not yet fixed)

- [ ] **Pulse volume & duty macros are probably still stomped.** `processBlock`
  calls `setPulseDuty(0/1)` **every block**, which re-writes `$4000`/`$4004`
  (duty **+ velocity-derived volume**) whenever a pulse note is active. A
  Vol Decay / Stab / Duty Sweep macro writes the same register at 60 Hz, but
  `setPulseDuty` runs ~86 Hz and re-asserts full velocity volume at the top of
  each block, overwriting the macro. The `51db06a` `active`-freeze fix is
  necessary but **not sufficient** for these. Expected symptom: Pulse 1/2 stays
  near full volume / fixed duty instead of fading or sweeping.
  **Likely fix:** make the per-block pulse-duty sync idempotent (only call
  `setPulseDuty` when the duty param actually changes), mirroring `51db06a`.

### Macro sequencer (per channel)
- [ ] **Vibrato** (pitch macro) on Pulse 1/2 — held note wobbles continuously
  (pitch macros write `$4002/3`, nothing stomps them → expected to work now).
- [ ] **Vol Decay / Stab** on **Noise** — should fade (`$400C` is not stomped). Confirm.
- [ ] **Vol Decay / Stab / Duty Sweep** on **Pulse 1/2** — see ⚠️ above; expected to fail until the duty-sync is gated.
- [ ] **Arp Major / Arp Minor** macro on a held note — cycles root / 3rd-or-4th / 5th.
- [ ] Which macro type affects which channel is correct: volume → Pulse + Noise only; duty → Pulse only; pitch/arpeggio → Pulse + Triangle + VRC6. (Triangle has no volume reg; VRC6 volume/duty macros are intentionally no-ops.)
- [ ] Macro resets on each note-on and stops on note-off (no release point → instant stop).
- [ ] Changing a macro preset mid-note behaves sanely (resets that channel's sequence).

### Standalone arpeggiator
- [ ] Up / Down / UpDown / Random produce the expected note order.
- [ ] **Random reorders once per cycle, not every frame** (the `51db06a` reshuffle fix).
- [ ] Octave range 1–4 expands the sequence correctly.
- [ ] Arp routes through the active voice mode; releasing all keys stops it (`arpNoteOff`).
- [ ] Arp on → the per-channel **macro** arpeggio is suppressed (no double-arping).
- [ ] Rapid chord changes don't leave a stuck note.

### Hardware sweep (Pulse 1 & 2)
- [ ] Enable + direction (Up/Down) + rate + shift → audible pitch glide on a sustained note.
- [ ] Disable → no sweep; note plays at fixed pitch.
- [ ] Very fast rate/shift doesn't silence the channel (period underflow mutes real hardware — confirm intended).

### Portamento / glide
- [ ] ⚠️ **Glide may only work in Unison mode.** `MacroEngine::noteOn` only starts a
  slide when the channel was already `active` with a valid `lastNote` (legato on
  the *same* channel). Round-Robin / Pitch-Split spread consecutive notes across
  *different* channels, so the new channel's `lastNote` is −1 → no glide. Verify
  in all three voice modes; if glide is wanted in non-unison modes it needs a
  mono voice path.
- [ ] Speed slider (1–255) changes glide rate as expected.
- [ ] First note after silence has no glide (nothing to glide from).

---

## P1 — Core regression surface

> `PluginEditor.cpp` (~412 lines) and `processBlock` were heavily reworked this
> phase, so re-confirm the basics.

### Channels — each produces correct sound
- [ ] Pulse 1, Pulse 2 (4 duty cycles, velocity → volume)
- [ ] Triangle (fixed volume by HW design)
- [ ] Noise (short/long mode, pitch-mapped period)
- [ ] DMC drum kit — GM map (36 Kick, 38 Snare, 42 Hi-Hat, toms, etc.); each note triggers the right sample
- [ ] VRC6 Pulse 1 / Pulse 2 (8 duty levels) and Sawtooth (accumulator-rate volume)

### Voice allocation
- [ ] Round-Robin cycles channels and steals oldest when full
- [ ] Pitch-Split routes low→Tri/Saw, high→Pulses at the split point
- [ ] Unison full-stack plays all *enabled* melodic channels; respects per-channel enable toggles
- [ ] VRC6 enable extends the voice pool from 3→6 in non-unison modes

### Audio output chain
- [ ] Master volume is smooth under fast automation (no zipper)
- [ ] DC blocker removes offset (no thump on note-off)
- [ ] Safety limiter never lets output NaN/clip harshly
- [ ] No crackle/dropouts at small buffer sizes / 44.1 & 48 kHz

### Visualizers
- [ ] All 7 oscilloscopes update and show the right waveform per channel
- [ ] Scopes stay zero-cross stable (no jitter) and don't lag the audio

### UI interaction (cannot be verified except by hand)
- [ ] All controls laid out without overlap/clipping at the default window size
- [ ] Macro selectors (7), sweep controls (Pulse 1 & 2), arp controls, portamento toggle + speed slider all present and bound to the right params
- [ ] Right-click context menus (Copy/Paste value, Set to Default) work on every control
- [ ] Double-click text entry on the knob and sliders
- [ ] Hover/focus visual states render
- [ ] Background image tiles correctly; scanline/CRT overlay intact
- [ ] On-screen keyboard plays and lights up held notes

### State & host
- [ ] Save/load (`getStateInformation`/`setStateInformation`) round-trips **all** params — including macro, sweep, portamento, arp — in a DAW session reload
- [ ] VST3 loads in a DAW; new params automatable; no thread-safety glitches under automation
- [ ] Phase 12: validate as VST3 in Pedalboard3 (MIDI note/CC; CPU < 5% target)

### NSF player (feature added this phase — full regression surface)
- [ ] LOAD a `.nsf` **and** a `.nsfe` → both parse; title/artist/©/chips populate.
- [ ] Playback is audible and correct; all 5 channel scopes (P1/P2/TRI/NSE/DMC) move.
- [ ] ◄ / ► SONG steps subsongs; counter shows n/total.
- [ ] PLAY / STOP transport freezes + resumes **without restarting** the tune.
- [ ] Window wears the Front-Loader skin, resizes within limits; all 3 themes apply.
- [ ] Synth ↔ NSF mode switch is clean both ways (no stuck notes — see P0 fix).
- [ ] EJECT opens the window; closing it returns to synth.
- [ ] A real bankswitched / multi-chip NSF (VRC6 etc.) plays correctly start-to-end.

---

## P2 — Deeper checks & known issues

- [ ] **Aliasing:** `Blip_Buffer` is configured but **not** used in the output
  path — `NessyAPU::process()` point-samples each core's `out[]` per host sample.
  Listen for aliasing on high notes / high duty; decide whether band-limiting is
  needed (and fix the ARCHITECTURE.md "Blip resamples" claim either way).
- [ ] **Register contention:** beyond the pulse-duty stomp (P0), audit other
  per-block syncs that re-write shared registers vs. the macro engine
  (e.g. `setNoiseMode` → `$400E`, VRC6 note-off writes).
- [ ] Sweep + pitch macro on the same pulse channel (both move period) — characterize the interaction.
- [ ] Portamento + pitch macro on the same channel (both add a period offset) — characterize.
- [ ] CPU usage under full 8-voice unison + macros + arp (target < 5% at 44.1 kHz stereo).
- [ ] Compiler warnings: C4244 `uint8_t`-from-`int`, and the `juce::ComboBox::label` deprecation seen in the Release build — confirm benign or clean up.
- [ ] Stress: MIDI all-notes-off / panic clears every channel and the arpeggiator.
- [ ] **NSF: dynamic expansion-chip scopes** — window shows 5 fixed 2A03 scopes;
  NSFs using VRC6/VRC7/N163/FDS/5B/MMC5 don't light extra scopes yet (deferred).
- [ ] **NSF: load-error surfacing** — a failed/malformed NSF load is currently
  swallowed (`loadNsf` TODO); the window keeps stale metadata. Verify once surfaced.
- [ ] **NSF: close-button UX (I4)** — closing the window leaves the engine live +
  `m_nsfPlaying=true`; re-opening via EJECT doesn't resume audio without a reload.
- [ ] **NSF: `prepareToPlay` re-init (I3)** — re-inits the song on every
  `prepareToPlay` (host transport start can restart the tune). Confirm acceptable.

---

## Verified (move items here once confirmed, with date + initials)

_(empty)_
