# NSF Player + Multi-Chip Synth Expansion — Design Spec

- **Date:** 2026-06-05
- **Status:** Approved design (pre-plan)
- **Project:** Nessy (GPL-3.0 NES 2A03 + expansion synth, JUCE 8, Windows/MSVC, C++20)
- **Supersedes/extends:** Phase 8–9 synth; reuses the Front-Loader UI and the `xgm::` NSFPlay device model already vendored.

## 1. Summary

Add a full **NSF (NES Sound Format) jukebox** to Nessy and, sharing the same chip cores, **expand the synth from 8 to ~28 channels** across all common NES expansion chips. The work is built on a single shared **NES chip foundation** (a real 6502 CPU + the five missing expansion chips) consumed by two engines: the NSF player (CPU-driven) and the existing synth (`NessyAPU`, MIDI/macro-driven).

The original request was "build an NSF player — should be simple." The catch discovered during design: Nessy's vendored `xgm::NES_CPU` is a **stub** (the cores came from *exotracker*, which, like FamiTracker, deletes the 6502 and synthesizes register writes in C++). An NSF is ripped 6502 machine code that must be *executed*; with a stubbed CPU, Nessy cannot run a single NSF instruction. So the real work is *finishing the emulator*, not a small add-on. The good news: `device.h` is pure NSFPlay (`IDevice`/`ISoundChip`/`Bus`/`Layer`), so the missing pieces drop onto interfaces already present, and the entire synth audio backend (Blip_Buffer, mixing, scopes, the cartridge UI) is reused.

## 2. Goals / Non-Goals

**Goals**
- Play standard and expansion NSF/NSFe files (all six expansion chips: VRC6, VRC7, FDS, MMC5, Namco 163, Sunsoft 5B) as a standalone jukebox inside the plugin.
- Expose every chip's channels as MIDI-playable synth voices (~28 total) under a unified voice-allocation pool.
- Keep the working synth path untouched until each piece is deliberately integrated (low regression risk).
- Lay this out on the existing Front-Loader deck (no new visual language).

**Non-Goals (v1)**
- NSF *export/authoring* (we play, not compose to NSF).
- `.m3u` playlists / multi-file batch.
- Host-transport-synced or MIDI-triggered NSF playback (jukebox transport only; revisit later).
- A wavetable/FM patch *editor* (chips use preset/ROM waveforms and patches in v1; custom-patch editing is a later enhancement).
- NSF2 advanced features beyond basic playback (IRQ-driven mixed code, etc.) — best-effort, warn if unsupported.

## 3. Scope decisions (locked during brainstorming)

| Decision | Choice |
|---|---|
| Chip support | **All** expansion chips (VRC6, VRC7, FDS, MMC5, N163, 5B) |
| NSF interaction | **Standalone jukebox**, free-running; plugin mode-switches Synth ↔ NSF |
| Build approach | **A** — slim engine on the existing `xgm` device model; vendor more of NSFPlay where cleaner than hand-porting |
| Spec scope | **Everything in one spec** — foundation + NSF player + full synth integration, delivered in phases |
| Chips reach | **Shared foundation** — cores used by both NSF engine and synth |
| Voice model | **Unified pool** — extend current Round-Robin/Pitch-Split/Unison over all enabled channels; per-chip enable groups; Noise/DPCM excluded from melodic allocation (percussion role) |
| UI | **A + C combo** — fixed 2A03 console deck + tabbed "cartridge expansion" bay, **plus** an always-on all-channels mini-scope strip for every enabled channel |

## 4. Architecture

```
                 ┌──────────────────────── shared NES chip foundation ───────────────────────┐
                 │  xgm::NES_CPU (real km6502)   xgm::Bus / Layer (device.h, already present) │
                 │  Sound chips (ISoundChip): NES_APU, NES_DMC, NES_VRC6  [existing]           │
                 │                            NES_MMC5, NES_FME7(5B), NES_FDS,                 │
                 │                            NES_N106(N163), NES_VRC7(+emu2413)  [new]        │
                 └───────────────▲───────────────────────────────────────▲────────────────────┘
                                 │ direct register writes                │ CPU-driven bus writes
                 ┌───────────────┴───────────────┐        ┌──────────────┴───────────────────┐
                 │  NessyAPU  (synth engine)      │        │  NsfEngine  (jukebox)             │
                 │  MIDI → VoiceAllocator → regs  │        │  NsfFile → NesBus+BankMapper →    │
                 │  Blip_Buffer render + mixing   │        │  NsfPlayer (INIT/PLAY @ 60Hz)     │
                 └───────────────┬────────────────┘        └──────────────┬────────────────────┘
                                 │                                        │
                          PlaybackMode {Synth, Nsf}  ── PluginProcessor::processBlock dispatches one
                                 │                                        │
                                 └──────────────► float L/R + per-channel scope taps ◄──────────┘
```

**Key principle:** one NES emulator in the binary, two ways to drive it. The chip *cores are shared code*, but each engine instantiates its **own** chip objects — mode-exclusive, so there is never shared mutable chip state. The synth pokes its chips directly (as it already does for VRC6); the NSF engine runs ripped 6502 code that pokes its chips through a bus. Only one engine is active at a time; the other is fully idle (zero CPU).

## 5. Components (purpose / interface / dependencies)

### 5.1 Shared foundation

- **`xgm::NES_CPU` (restored).** Replace the stub with NSFPlay's real km6502-based core. *Interface:* `IDevice` + `Exec(cycles)`/`SetPC`/registers, IRQ + `StealCycles`. *Dependency risk:* the synth's `NES_DMC` holds a `NES_CPU*` and calls only `StealCycles`/`UpdateIRQ`; these must remain cheap no-ops when the CPU is idle (Phase A gate: confirm synth still builds + sounds identical).
- **Expansion chips (new), each an `xgm::ISoundChip`:** `NES_MMC5` (2 pulses + PCM), `NES_FME7` (Sunsoft 5B — 3 PSG squares, env/noise), `NES_FDS` (1 wavetable + mod), `NES_N106` (Namco 163 — 1–8 multiplexed wavetable), `NES_VRC7` (6-ch 2-op FM via `emu2413` OPLL). *Interface:* `Write/Read/Tick/Render/SetClock/SetRate/SetMask`. *Dependency:* `emu2413` (VRC7), `emu2149` (optional, for 5B PSG) — small standalone cores.

### 5.2 NSF engine (`src/nsf/`)

- **`NsfFile`** — parsed NSF/NSFe. *Interface:* `static Result<NsfFile> parse(span<byte>)`; fields: load/init/play addr, song count + start, region flags, bank-init[8], expansion-chip flag byte, metadata strings, program blob. *Deps:* none (pure parser).
- **`BankMapper : IDevice`** — maps 4 KB NSF banks into the CPU window per bank-init + runtime `$5FF8–$5FFF` writes (flat load if no banking; FDS maps `$6000+`). *Interface:* `Write/Read`. *Deps:* `NsfFile` program blob.
- **`NesBus : xgm::Layer`** — CPU address routing: `$0000–07FF` RAM (mirrored), `$6000–7FFF` WRAM, `$4000–4017` → APU/DMC, expansion register ranges → active chip(s), `$8000–FFFF`(+`$6000` FDS) → `BankMapper`, `$5FF8–5FFF` → bank writes. *Deps:* chips, `BankMapper`.
- **`NsfPlayer`** — the loop. *Interface:* `load(NsfFile)`, `selectSong(n)` → `initSong` (set A=song, X=region, PC=init, push sentinel, run CPU to RTS/cap), `render(float* L, float* R, int n)` (advance chips clock-by-clock, fire PLAY each NTSC/PAL frame, mix NSFPlay-native), per-channel taps for scopes. *Deps:* CPU, NesBus, chips.
- **`NsfEngine`** — JUCE-facing façade owning the above + transport state, and **its own chip instances** (constructed per loaded file from the NSF's chip flags — independent of `NessyAPU`'s). *Interface:* `prepare(sampleRate)`, `loadFromFile(File)` (message thread), `render(buffer)` (audio thread), `play/stop/next/prev/selectSong`, `getMetadata()`, `getActiveChannels()`. *Deps:* all of `src/nsf/`.

### 5.3 Synth integration (extends existing)

- **`NessyAPU`** grows to optionally instantiate + render the new chips and route MIDI register writes to them (mirrors current VRC6 handling); per-channel taps extend to ~28.
- **`VoiceAllocator`** extends the unified pool over all enabled channels; adds **per-chip enable groups**; generalizes Pitch-Split; Noise/DPCM flagged non-melodic (percussion role, excluded from melodic allocation).
- **`MacroEngine`** instances extend to the new melodic channels (existing 8 presets apply where meaningful).
- **APVTS** gains per-chip enable params, per-channel params (duty/patch/wave/volume), and `playbackMode`.

## 6. Data flow

**Synth mode:** `processBlock` → MIDI → `VoiceAllocator` → `NessyAPU::writeRegister` per channel → `clockAPU` + Blip_Buffer → mix → float L/R; per-channel `out[]` → scope ring buffers. (Unchanged path, more channels.)

**NSF mode:** `processBlock` → `NsfEngine::render(L,R,n)` → per output sample, advance CPU+chips the right CPU-clock count, calling `PLAY` at frame boundaries; each chip `Render()`'d and summed with NSFPlay mixing → float L/R; per-channel taps → scope ring buffers (only *enabled/active* channels populate the all-channels strip).

## 7. Voice model (synth)

- All **enabled** channels form one allocation pool; the existing three modes operate over it:
  - **Round-Robin** cycles through enabled melodic channels.
  - **Pitch-Split** generalizes from the current low/high Tri-Saw/Pulse split to configurable split points across the enabled set.
  - **Unison** stacks the note on all enabled melodic channels.
- **Per-chip enable groups:** toggling a chip on/off adds/removes its channels from the pool in one action.
- **Non-melodic channels** (2A03 Noise, DMC; 5B noise/env) are excluded from melodic allocation and addressed separately (percussion / manual).

## 8. UI design (Phase D + NSF-mode screen)

Extends the Front-Loader deck — **no new visual language**.

- **Fixed 2A03 console** — the five base strips (P1 P2 TRI NSE DMC) always visible, full-size, exactly as today.
- **Tabbed cartridge-expansion bay** — one framed bay below the console with chip tabs (VRC6 / MMC5 / 5B / FDS / N163 / VRC7). Selecting a tab shows that chip's full-size strips. Each tab carries a red **enable LED**: a chip can be enabled and sounding in the pool while you view a *different* chip's controls (enable ≠ view). FM (VRC7) strips show a **PATCH** selector where pulses show duty.
- **All-channels mini-scope strip** — a thin, always-on row of small zero-cross scopes for **every enabled channel** across all chips, so the full mix is watchable while editing one chip. Reuses `nessy::NessyScope`/`gm::ui::Oscilloscope`.
- **NSF mode** reuses the same furniture: the cartridge becomes the **NSF loader** (file open, subsong ◄/►, PATCH n/total → track n/total, title/artist/copyright readout); on load, the bay auto-selects the song's expansion chip and the scope strip auto-populates with the song's active channels. A Synth↔NSF switch lives in the header/system area.

## 9. Reference-code-first provenance

Per project policy, all vendored cores are recorded before copying.

- **Primary reference:** the NSFPlay core family — the lineage behind Nessy's existing `xgm::` cores (via Dn-FamiTracker / exotracker). Phase A selects the exact upstream that **version-matches** the already-vendored chips so the CPU + new chips reconcile cleanly, using canonical NSFPlay for the NSF player/parser/bus that trackers omit. Files to port: `nes_cpu.*` + `km6502.*` (CPU), the NSF/NSFe parser + bus/player logic (`nsf.*`, `nes_bus.*`, `nsfplay`-player load/init/play), and the five chip cores `nes_fds.*`, `nes_vrc7.*`, `nes_mmc5.*`, `nes_n106.*`, `nes_fme7.*`.
- **Secondary:** `emu2413` (OPLL, VRC7) and `emu2149` (PSG, 5B) by Mitsutaka Okazaki.
- **Reuse mode:** direct-copy for the chip cores (match existing vendored style); close-port for the player/bus/parser to fit Nessy's façade.
- ⚠️ **Phase A gates (must complete before copying):** pin the exact upstream **commit SHA**; **confirm each license** at that commit (NSFPlay believed GPL-2.0 — compatible with Nessy GPL-3.0; `emu2413`/`emu2149` believed MIT) via the `licensing` skill; record repo/SHA/license/files/reuse-mode in this spec and in `THIRD_PARTY_LICENSES.md`; preserve attribution + license texts.

## 10. Phased roadmap (one spec, staged delivery)

- **Phase A — Foundation.** Restore real `NES_CPU`; vendor + compile the 5 chips + `emu2413`/`emu2149`; smoke-test each chip by direct register writes; validate the CPU against a known 6502 test ROM (e.g., Klaus Dormann functional test). Confirm the synth is byte-identical (CPU restore doesn't disturb `NES_DMC`). *No UI.*
- **Phase B — NSF jukebox.** `NsfFile`/NSFe parser, `BankMapper`, `NesBus`, `NsfPlayer`, `NsfEngine`, `playbackMode` switch, RT-safe file load, NSF-mode UI (cartridge → loader + subsong nav + metadata + auto-populated scope strip). **Ships the requested feature and exercises every chip against real NSFs.**
- **Phase C — Synth chip expansion.** Wire chips into `NessyAPU` + `VoiceAllocator` (unified pool, per-chip enable groups) + params/macros. **Infra-first sub-phasing** (decided 2026-06-08, see §10a):
  - **C.1 — Channel/voice infra refactor.** Data-driven channel registry + generalize `VoiceAllocator` to N channels; **no new chip audio, no new params, no UI**. Behavior-preserving for today's 2A03+VRC6 set; verified by a new Catch2 `VoiceAllocator` suite + regression assertion.
  - **C.2 — Easy chips.** MMC5 (2 pulses) + Sunsoft 5B (3 squares) audio in `NessyAPU` + params/macros, allocated through the new pool.
  - **C.3 — Wavetable chips.** FDS + Namco 163 (up to 8 ch) audio + wavetable params.
  - **C.4 — FM.** VRC7 + custom-patch model (highest risk; tail).
- **Phase D — Multi-chip synth UI.** The A+C deck (fixed console + tabbed cartridge bay) + the all-channels mini-scope strip; per-chip param controls.

Ordering front-loads the jukebox and de-risks the chips (real NSFs stress them) before the synth integration depends on them.

### 10a. Phase C.1 design — channel/voice infra refactor (infra-first)

Decided via brainstorm (2026-06-08): generalize the voice/channel layer **before** any new chip audio, to quarantine the 8→~28-channel `VoiceAllocator` change from chip integration. No audible change lands in C.1.

- **Channel registry** — a single `constexpr` source of truth listing every channel (2A03, VRC6, and the five future chips) as `ChannelDesc { id, chipGroup, kind (square/triangle/saw/wavetable/FM/noise/dpcm), role (melodic/percussion), splitCapable }`. Future chips appear in the table but their groups default **disabled**. Replaces the hardcoded `array<int,6>` channel order and `NUM_TOTAL_VOICES = 8`.
- **VoiceAllocator generalized** — iterates the registry filtered to the **active set** (enabled chip groups ∩ melodic). Round-Robin / Pitch-Split / Unison / voice-steal all operate over that set. Pitch-Split keeps a **single** split point partitioning active melodic channels into low/high-capable sets (richer multi-tier splits are a non-goal). Noise/DPCM (and 5B noise/env later) stay non-melodic.
- **Behavior preservation** — with only the 2A03 + VRC6 groups enabled (today's exact set), allocation is **identical** to current; `NessyAPU` is untouched.
- **Verification (unit tests + regression)** — a new Catch2 `VoiceAllocator` suite covering allocation order, voice-steal, pitch-split partition, unison stacking, per-group gating, and non-melodic exclusion, plus a regression case asserting the legacy 8-channel set matches today's behavior.
- **Out of scope for C.1** — no new APVTS params, no new chip audio, no UI change. Rejected alternatives: per-chip allocator modules (over-engineered for a fixed chip set) and growing the enums/arrays with conditionals (a 28-channel tangle that fights the test goal).

## 11. Threading / RT-safety

- Parse + ROM-load + per-file chip-set construction happen on the **message thread** into a ready `LoadedNsf` state; handed to the audio thread by an **atomic pointer swap**; old state freed back on the message thread. The audio thread never parses or allocates.
- Transport (play/stop/subsong) and `playbackMode` are atomics read at the top of `render`.
- 6502 + chip stepping in `processBlock` is cheap (~30k cyc/frame); acceptable RT cost (NSFPlay does this in real time).
- Allocation-trapping test harness around `processBlock` in NSF mode (Roadmap Phase 13 alignment).

## 12. Error handling

- Invalid NSF (bad magic, truncated, bad addresses) → reject, surface a UI message, stay in the current mode. No partial loads reach the audio thread.
- CPU runaway (INIT/PLAY never returns) → per-call **cycle cap**; abort the frame and flag rather than hang the audio thread.
- Unknown chip-flag bits / unsupported NSF2 features → ignore/warn, play what's supported.
- Mode switch mid-playback → clean stop of the outgoing engine, reset of the incoming.

## 13. Testing strategy

The engine is deterministic, so this is the project's opportunity to start the automated suite (Roadmap Phase 13):

- **6502 CPU:** run Klaus Dormann's functional test + `nestest` log compare — gold-standard correctness.
- **NSF/NSFe parser:** unit tests on crafted valid/invalid/edge headers and chunks.
- **BankMapper:** bank-init + `$5FFx` write mapping unit tests.
- **Chips:** smoke tests (register → expected level/sign); golden output compare vs upstream NSFPlay for a few reference NSFs (audio hash).
- **Player:** load a known NSF, render N frames, assert a register-write sequence or audio hash.
- **RT-safety:** allocation-trap harness around NSF-mode `processBlock`.
- **Mutation pass** (per project Test Quality rule) on the parser + bank logic.

## 14. Risks / open questions

- **VRC7 FM (emu2413)** integration + the custom-patch model is the highest effort/risk core. (Phase C tail.)
- **Namco 163** time-multiplexes 1–8 channels — affects mixing/timing and the dynamic scope count.
- **VoiceAllocator at ~28 channels** — Pitch-Split generalization and pool ergonomics need care. *Addressed by the C.1 infra-first refactor (data-driven registry + unit tests); see §10a.*
- **CPU-restore regression** — must prove the synth is unaffected (Phase A gate).
- **Binary size / build time** grow with the CPU + 5 chips.
- **License confirmation** pending the pinned commit (Phase A gate).

## 15. Definition of done (per phase)

- **A:** all chips compile + emit sound via direct writes; 6502 passes the test ROM; synth unchanged. 
- **B:** standard + each-expansion NSF plays correctly from the jukebox UI; subsong nav + metadata + scopes work; RT-safe load verified; manual + automated checks pass.
- **C.1:** data-driven channel registry + N-channel `VoiceAllocator` land; legacy 2A03+VRC6 allocation is behavior-identical; new Catch2 `VoiceAllocator` suite passes; no new chip audio/params/UI; `NessyAPU` untouched.
- **C:** each chip playable via MIDI under all three voice modes with per-chip enable; params/macros functional.
- **D:** the full deck + scope strip ship; manual UI verification by the user (UI rule).
