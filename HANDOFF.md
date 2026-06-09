# Nessy — Project Handoff

> **Purpose:** orient whoever picks this up next — a developer **or** a fresh AI-agent session — with enough context to be productive immediately. Written 2026-06-09, at the end of Phase C.2.
>
> **Read order for a cold start:** this doc → `STATE.md` (current snapshot) → `ARCHITECTURE.md` (how it works) → `docs/superpowers/specs/2026-06-05-nsf-player-multichip-synth-design.md` (the multi-chip vision + locked decisions) → `ROADMAP.md` (what's next) → `TESTLATER.md` (what still needs a human's ears/eyes).

---

## 1. TL;DR — where we are

Nessy is a **GPL-3.0 VST3 + Standalone NES audio plugin** (JUCE 8, C++20, Windows/MSVC, CMake + CPM). It started as an 8-channel 2A03+VRC6 synth and has grown into **two instruments sharing one NES chip foundation**:

1. A **13-channel NES synthesizer** (MIDI-played): 2A03 (Pulse 1/2, Triangle, Noise, DMC) + VRC6 (Pulse 1/2, Saw) + MMC5 (Pulse 1/2) + Sunsoft 5B (Square A/B/C), with hardware macros, arpeggiator, sweep, and portamento.
2. An **NSF/NSFe player**: load a NES music file and it plays in a themed floating window with metadata, transport, subsong nav, and live channel scopes.

**Current branch:** `feat/nsf-multichip` (NOT merged to `master` — see §10). **Build:** green (Standalone + VST3). **Tests:** 26 Catch2/CTest, all passing. **Last work:** Phase C.2 complete (MMC5 + 5B as synth voices), then a full docs sync.

**The big caveat:** the automated tests cover CPU/chip/NSF-engine/allocator *logic*. The actual *audio and UI* (does it sound right, do the macros modulate, are mix levels balanced) is **verified by ear only** — and there's a backlog of pending by-ear checks in `TESTLATER.md`. Everything is "tests-green, ears-pending."

---

## 2. How to build, test, and run

```bash
# Configure (first time)
cmake -B build -G "Visual Studio 17 2022"

# Build everything (Standalone + VST3 + tests)
cmake --build build --config Release

# Build just the standalone
cmake --build build --config Release --target Nessy_Standalone

# Build + run the test suite (26 tests)
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```

**Output paths:**
- Standalone: `build\Nessy_artefacts\Release\Standalone\Nessy.exe`
- VST3: `build\Nessy_artefacts\Release\VST3\Nessy.vst3` (auto-installed to `%COMMONFILES%\VST3`)
- Tests: `build\tests\Release\nessy_tests.exe`

**Prerequisites:** Visual Studio 17 2022 + CMake (the tested toolchain). And the sibling MIT repo **ghostmoon-oss** — the skin-neutral DSP utils + `gm::ui::ScaledEditor`/`Oscilloscope`, spun out of a proprietary plugin-UI library — must be present at `../ghostmoongpl` (a folder named `ghostmoongpl` **next to** this repo). You must obtain it from its own repository *before* configuring; if you don't have the source, ask the project owner (see `HOWTO.md` for the prerequisite note). Nessy links `ghostmoon_oss::dsp` + `ghostmoon_oss::core`; do **NOT** link the proprietary `ghostmoon` (GPL violation).

**CTest gotcha:** CTest filters by test *name*, not Catch2 *tag*. `-R voicealloc` matches nothing (the tag is `[voicealloc]`); use the full suite or a name substring like `-R "channel registry"`.

**Running the new chips / NSF (no dedicated UI yet — that's Phase D):**
- **MMC5 / 5B synth voices:** enable via the APVTS params `mmc5Enable` / `sunsoft5bEnable` (and `mmc5Pulse1Duty`/`mmc5Pulse2Duty`). The Front-Loader editor has **no controls** for these yet (Phase D), so reach them through a host's **generic parameter view**: load the **VST3 in a DAW** (e.g. JUCE's free bundled `AudioPluginHost`, or Reaper). ⚠️ **In the Standalone there is currently no way to enable these chips** (no UI, no generic param view) — use the VST3 until the Phase D UI lands. New chip groups default **off**, so the synth is unchanged until you enable them.
- **NSF player:** click the **EJECT** chip on the cartridge in the editor → a floating "NSF PLAYER" window opens → **LOAD** an `.nsf`/`.nsfe`. Test files are in `test-nsfs/` (gitignored).

**MANDATORY build-preservation discipline:** before any full-plugin `cmake --build`, copy the current `Nessy.exe`, `Nessy.vst3` (bundle dir), and `nessy_tests.exe` into `releases/<YYYY-MM-DD_HHMM>/` and verify the copy (confirm the new dir actually contains all three: `Nessy.exe`, the `Nessy.vst3/` bundle **directory**, and `nessy_tests.exe`). (See existing backups under `releases/`.) Building only `--target nessy_tests` doesn't touch the plugin artifacts, so preservation isn't needed for test-only builds.

---

## 3. Architecture in one screen

(Full detail in `ARCHITECTURE.md`. This is the orientation version.)

```
                          ┌─ Synth path  : MIDI → VoiceAllocator → NessyAPU ─┐
PluginProcessor::processBlock ┤  (playbackMode switch)                        ├→ shared DSP tail → output
                          └─ NSF path    : NsfEngine.renderSamples ──────────┘   (ghostmoon-oss DC block / limiter / volume)
```

- **`PluginProcessor`** branches on an atomic `playbackMode` (0 = Synth, 1 = NSF) at the top of `processBlock`. Both paths feed a shared DSP tail.
- **Synth path:** MIDI → **`VoiceAllocator`** (registry-driven, N-channel) → **`NessyAPU`** (owns the 5 chip cores) → per-sample clock + mix.
- **`VoiceAllocator`** is **data-driven**: `src/apu/ChannelRegistry.h` is a `constexpr` table of `ChannelDesc{ id, chipGroup, kind, role, splitTier }` — the single source of truth for the channel set. The allocator routes notes over the **active set** (enabled chip groups ∩ melodic channels) in Round-Robin / Pitch-Split / Unison modes, and drives sound through the **`IVoiceSink`** interface (3 methods: `noteOn`/`noteOff`/`isChannelEnabled`) that `NessyAPU` implements. This decoupling lets the allocator unit-test with no JUCE/NSFPlay link.
- **`NessyAPU`** owns 5 NSFPlay cores → **13 channels**: `NES_APU` (P1/P2), `NES_DMC` (Tri/Noise/DMC), `NES_VRC6` (P1/P2/Saw), `NES_MMC5` (P1/P2), `NES_FME7` (5B A/B/C). External chips (VRC6/MMC5/5B) follow a common pattern: construct → `SetClock`/`SetRate`/`Reset` → `Tick` when the group is enabled → register writes in `noteOn`/`noteOff` → `Render()` + linear-mix in `process()`.
- **Mixing reality (important):** `NessyAPU::process()` advances cores per host sample and **point-samples** each core's `out[]`/`Render()` — 2A03 via the NESdev non-linear pulse+TND formula, VRC6/MMC5/5B summed linearly with per-chip scale divisors. **`Blip_Buffer` is configured but NOT used in the output path** (band-limiting / high-note aliasing is a known open item — see `TESTLATER.md` P2). Do not assume Blip resampling.
- **NSF path:** **`NsfEngine`** (`src/nsf/NsfEngine`, PIMPL) parses NSF/NSFe and runs a **real 6502** (`xgm::NES_CPU` / km6502) over the song's INIT/PLAY against the NSFPlay chip bus, tapping per-channel `out[]` for scopes. Loading is **RT-safe**: the engine is built on the message thread, swapped in via an atomic pointer (`m_pendingNsf` → `m_activeNsf`), and the old one is retired/deleted on the editor's 60 Hz timer (`m_retireNsf` + `retireOldEngine`). The audio thread never allocates, locks, or deletes.

---

## 4. What's been built (phase by phase)

The granular history is in `CHANGELOG.md`; the design rationale is in the spec (§7 below). Summary:

| Phase | What | Key files | Status |
|---|---|---|---|
| (pre-NSF synth) | 8-channel 2A03+VRC6 synth; MacroEngine (8 presets), Arpeggiator, hardware sweep (P1/P2), portamento; Front-Loader NES UI (3 themes); migrated off proprietary ghostmoon → ghostmoon-oss (MIT) | `PluginProcessor`, `PluginEditor`, `NessyUI.h`, `MacroEngine`, `Arpeggiator` | ✅ done |
| **A — Foundation** | Restored the **real `NES_CPU`** (it was a no-op stub); vendored 5 expansion-chip cores (MMC5, FDS, N163/N106, VRC7, FME7) + MIT `emu2413`/`emu2149`; added Catch2/CTest; validated the 6502 against the **Klaus Dormann** functional test | `src/apu/nsfplay/...` (vendor), `tests/cpu/` | ✅ done |
| **B — NSF player** | `NsfEngine` (PIMPL slim engine), processor `playbackMode` + RT-safe load, themed `NsfPlayerWindow` (load/metadata/transport/subsong/live scopes). Then a review pass fixed 5 real bugs (scope-read data race, FileChooser UAF, audio-thread alloc, song clamp, stuck-note transition) | `src/nsf/NsfEngine.*`, `src/NsfPlayerWindow.*`, `src/nsf/NsfMix.h` | ✅ done, ears-pending |
| **C.1 — Channel/voice infra** | Made `VoiceAllocator` data-driven (`ChannelRegistry` + `IVoiceSink`), N-channel, per-chip-group enables. **Behavior-identical** for the 2A03+VRC6 set; new Catch2 `VoiceAllocator` suite | `src/apu/ChannelRegistry.h`, `src/apu/IVoiceSink.h`, `src/apu/VoiceAllocator.*`, `tests/cpu/test_voice_allocator.cpp` | ✅ done |
| **C.2 — Easy chips** | MMC5 (2 pulses) + Sunsoft 5B (3 squares) as MIDI synth voices, **full parity** (enable params, MacroEngine, portamento). Channel count 8 → 13 | `src/apu/NessyAPU.*`, `src/apu/MacroEngine.*`, `PluginProcessor.cpp`, `ChannelRegistry.h` | ✅ done, ears-pending |

The 13 channels and their ids (used everywhere — registry, enum, `noteOn`): `0` Pulse1, `1` Pulse2, `2` Triangle, `3` Noise, `4` DMC, `5` VRC6_P1, `6` VRC6_P2, `7` VRC6_Saw, `8` MMC5_P1, `9` MMC5_P2, `10` FME7_A, `11` FME7_B, `12` FME7_C.

---

## 5. Key files map

| File | Role |
|---|---|
| `src/PluginProcessor.h/.cpp` | JUCE processor; APVTS params; `processBlock` (Synth/NSF branch); NSF load + RT-safe engine swap |
| `src/PluginEditor.h/.cpp` | Front-Loader NES UI; EJECT → NSF window; 60 Hz timer (scopes + `retireOldEngine`) |
| `src/NessyUI.h` | `nessy::NessyLookAndFeel` (NES skin) + `nessy::NessyScope` + shared chrome helpers |
| `src/NsfPlayerWindow.h/.cpp` | Themed floating NSF-player window (`NsfPlayerWindow` + `NsfPlayerView`) |
| `src/apu/NessyAPU.h/.cpp` | APU wrapper: 5 chip cores, register writes, point-sampled mixing; implements `IVoiceSink` |
| `src/apu/VoiceAllocator.h/.cpp` | Registry-driven N-channel MIDI→channel routing; 3 modes |
| `src/apu/ChannelRegistry.h` | `constexpr` `ChannelDesc` table — the channel-set source of truth |
| `src/apu/IVoiceSink.h` | 3-method allocator↔APU boundary (decouples + enables unit tests) |
| `src/apu/MacroEngine.h/.cpp` | 60 Hz macro sequencer (all 13 channels); `applyMacroTick` → `NessyAPU::applyMacroVolume/Duty` |
| `src/apu/Arpeggiator.h/.cpp` | 60 Hz held-note arpeggiator |
| `src/apu/NessyMemory.h` | Virtual NES $8000–$FFFF space + 6 factory DPCM samples |
| `src/nsf/NsfEngine.h/.cpp` | NSF/NSFe engine (PIMPL): parse + 6502 INIT/PLAY + chip bus + per-channel scopes |
| `src/nsf/NsfMix.h` | mono int16 → stereo float bridge |
| `src/apu/nsfplay/` | NSFPlay cores + km6502 CPU — **read-only vendor code** |
| `src/apu/blip_buffer/` | Blargg Blip_Buffer — vendored, **configured but not in the synth output path** |
| `tests/cpu/` | Catch2 suite: km6502 (Klaus Dormann), expansion-chip smoke, NSF engine/bridge, VoiceAllocator |
| `docs/superpowers/specs/2026-06-05-...-design.md` | The multi-chip + NSF **design spec** (the master vision) |
| `docs/superpowers/plans/2026-06-0[7-9]-...md` | Per-phase implementation plans |

---

## 6. How work is done here (conventions)

This project follows a strict, repeatable rhythm. **Follow it for new features.**

1. **brainstorming → writing-plans → subagent-driven-development.** These are skills (slash-commands) in the Claude Code agent harness; their output artifacts live in `docs/superpowers/` (specs + plans). A human developer without the harness can follow the same process by hand. Each phase: brainstorm the design with the user → record decisions in the spec → write a code-complete plan → execute it task-by-task with **fresh subagents** (one implementer per task + a **spec-compliance review** + a **code-quality review**, fix loops until both pass), then a **final holistic review**.
2. **The spec is the master plan.** `docs/superpowers/specs/2026-06-05-nsf-player-multichip-synth-design.md` holds the whole vision (all chips, the unified voice pool, the Phase D UI design in §8, the phased roadmap in §10, plus §10a (C.1 detail) and §10b (C.2 detail)). Each new sub-phase adds a §10x detail section + a definition-of-done in §15.
3. **Commit discipline:** conventional commits (`feat:`/`fix:`/`docs:`/`refactor:`/`test:`), no emojis, end the body with `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`. Update the affected docs **in the same change** (CHANGELOG always; STATE/ROADMAP/etc. as relevant). Commit/push only when the user asks.
4. **The 6 standard docs** (CHANGELOG, STATE, ROADMAP, ARCHITECTURE, HOWTO, README) are mandatory and kept current. Plus **`TESTLATER.md`** — the manual-test backlog (audio/UI can't be auto-verified, so it accumulates there). Its items are tiered **P0** (recently changed / check before trusting), **P1** (core regression surface), **P2** (deeper checks & known-issue follow-ups).
5. **Reference-code-first** for the remaining chips: before writing chip integration, read the vendored NSFPlay core + the existing smoke test to get the exact register map and period formula (don't guess). Record provenance in `THIRD_PARTY_LICENSES.md`.
6. **RT-safety is non-negotiable** on the audio thread (`processBlock`, `NessyAPU::process/clockAPU/noteOn/noteOff`, `MacroEngine::tick`): no allocation, no locks, no deletes. Engine swaps use atomic pointers; deletes happen on the message thread.
7. **Verify external library APIs with Context7** before writing JUCE/CMake/etc. calls (don't rely on training data).
8. **GPL-3.0** (forced by NSFPlay). Never link the proprietary `ghostmoon`; ghostmoon-oss (MIT) only.

---

## 7. Gotchas & hard-won lessons (read before touching the relevant area)

- **The DMC needs a non-null CPU or the synth crashes at startup (0xC0000005).** `NES_DMC` calls `cpu->UpdateIRQ()`/`StealCycles()` *unguarded*, including inside `Reset()`. `NessyAPU` constructs an idle `NES_CPU` and calls `m_apu2->SetCPU(m_cpu.get())` **before** `Reset()`. The idle CPU is never `Exec()`'d (behaviour-neutral). The stub CPU's inline no-ops once hid this; the real CPU's out-of-line methods crash. *(This crash slipped past a green test suite because the DMC was untested — there's now a `[chips][dmc]` regression test.)*
- **The 5B period numerator is `1789772`, NOT `1789772.7` — do not "fix" it.** `NES_FME7` ignores `SetClock`/`SetRate` and runs at a hardcoded `DEFAULT_CLOCK = 1789772`. `midiToFME7Period` uses `1789772 / (32 * freq)` (no `-1`) to match the chip's fixed clock. Changing it to the NTSC `1789772.7` detunes the 5B. (Pitch was empirically verified correct — A4 = 440 Hz — by compiling emu2149 and measuring.)
- **`midiToPeriod` is chip-aware.** MMC5 is 11-bit (clamp 2047) like the 2A03 — NOT 12-bit like VRC6. 5B uses a completely different (AY) formula via `midiToFME7Period`. `midiToPeriod` dispatches FME7 channels to `midiToFME7Period` (so MacroEngine portamento gets the right base). The VRC6 clamp is 12-bit (4095).
- **5B mixer register bits are inverted (AY semantics):** in reg `$07`, `1` = tone/noise *disabled*. `NessyAPU` tracks `m_fme7Mixer` (starts `0x3F` = all off), clears a channel's tone bit on noteOn, sets it on noteOff. 5B amplitude writes must keep D4 = 0 (fixed level, not hardware envelope).
- **Macro register routing is channel-aware in `NessyAPU`.** `MacroEngine::applyMacroTick` calls `NessyAPU::applyMacroVolume/applyMacroDuty` (which route per chip) + `writeNoteRegisters`/`writePitchOffset` (chip-aware switches). When adding a chip, extend all four.
- **Known unfixed: the per-block pulse-duty stomp.** `processBlock` pushes `setPulseDuty`/`setMmc5PulseDuty` *every block*, which re-asserts the volume/duty register and can stomp a running Vol-Decay/Stab/Duty-Sweep *macro* on 2A03 **and** MMC5 pulses. The fix is to make those setters idempotent (only write on actual param change), mirroring the `51db06a` macro/arp fix. Tracked in `TESTLATER.md`.
- **JUCE header leakage:** the km6502 macros (`External`/`Callback`/`Inline`/`CCall`/`FastCall`) and NSFPlay headers must not reach JUCE TUs. Solved by the **PIMPL** in `NsfEngine` (its header includes only `<cstdint>/<memory>/<string>`). Keep it that way.
- **Behaviour preservation is sacred for the synth.** Any allocator/macro/APU refactor must keep the existing 2A03+VRC6 register writes byte-identical — verified by **comparing against the pre-refactor code during review** (the allocator unit tests cover note→channel *routing*, not the literal register bytes). New behaviour goes behind new, default-off groups/params.
- **Audio/UI bugs can't be marked "verified" by automated tests** — they need the user's ears/eyes. That's why `TESTLATER.md` exists and why audio tasks end with a "by-ear" gate.

---

## 8. Open issues, risks & pending verification

- **By-ear backlog (`TESTLATER.md` P0):** the NSF-player review fixes, the C.1 regression check, and the entire C.2 chip set (MMC5 duty, 5B pitch, macros/portamento on the new channels, mix-level balance, 2A03/VRC6-unchanged regression) are all **tests-green but ears-pending**. A focused listening pass is overdue.
- **Mix-level calibration:** the external-chip mix divisors are starting values to tune by ear — VRC6 `/65536`, MMC5 `/65536`, 5B `/8000`. Balance them against the 2A03.
- **Per-block duty-stomp** (see §7) — affects 2A03 + MMC5 pulse macros.
- **Blip_Buffer / aliasing:** the synth point-samples `out[]` rather than band-limiting; high notes / high duty may alias. Decide whether to wire Blip or accept it.
- **`THIRD_PARTY_LICENSES.md` ⚠️ placeholders:** two unresolved before any release — the JUCE license tier, and the pinned Dn-FamiTracker/NSFPlay commit SHA (the SHA pins the exact upstream the cores were copied from, needed for attribution + GPL compliance). Confirm via the `licensing` skill.
- **Branch not merged:** `feat/nsf-multichip` carries everything since the synth phases and has **not** been merged to `master`. The user wants this coordinated with the ghostmoon-oss sibling repo (as a prior merge was). Defer until a sensible milestone.
- **Portamento limitation** (pre-existing): glide reliably works only in Unison mode (in Round-Robin/Pitch-Split consecutive notes land on different channels with no `lastNote` to glide from). Tracked in `TESTLATER.md`.
- **Deferred NSF-window polish:** dynamic expansion-chip scopes (the window shows 5 fixed 2A03 scopes), close-button UX (I4), and load-error-to-UI surfacing.

---

## 9. What's next (the road ahead)

The user was about to start **Phase D** when this handoff was requested. The open options (from `ROADMAP.md` + the spec §10):

- **Phase D — Multi-chip synth UI** *(strong candidate to do next)*. The chip set has outgrown "toggle params in a DAW." The spec §8 designs it: a **fixed 2A03 console** (the 5 base strips, always visible) + a **tabbed cartridge-expansion bay** (chip tabs VRC6 / MMC5 / 5B / FDS / N163 / VRC7, each with an enable LED; FM tabs show a PATCH selector where pulses show duty) + an **all-channels mini-scope strip**. NSF mode reuses the cartridge as the loader. Doing D before C.3/C.4 makes the already-built chips playable/visible in the Standalone.
- **Phase C.3 — Wavetable chips (FDS + Namco 163).** Medium difficulty. **FDS:** 1 channel, 64-step wave RAM (`$4040–$407F`), `$4080` vol env, `$4082/3` freq, `$4089` wave-write mode (see the FDS smoke test in `tests/cpu/test_expansion_chips.cpp`). **N163 (`NES_N106`):** **1–8 time-multiplexed channels** — affects mixing/timing and means a *dynamic* channel count (the `ChannelRegistry`/`VoiceAllocator` currently assume a **fixed** set; this is the main design wrinkle — resolve it in the C.3 brainstorm **before** writing code). Register pointer `$F800` + data `$4800`; per-channel regs at the top of wave RAM. Both need wavetable params (and the registry needs `ChannelKind::Wavetable` rows).
- **Phase C.4 — VRC7 FM (highest risk / effort).** OPLL via MIT `emu2413`. `$9010` address + `$9030` data; 6 FM channels; 15 built-in instrument patches + 1 custom patch. The **custom-patch model** is the hard part. Sequence it last.
- **Smaller items:** the by-ear pass, the duty-stomp idempotency fix, the deferred NSF-window polish, the THIRD_PARTY_LICENSES placeholders.

When starting any of these: brainstorm → record a §10x spec section + DoD → write a code-complete plan → subagent-execute with spec+quality reviews. Read the vendored core + smoke test first (reference-code-first).

---

## 10. Provenance & licensing (for release)

- **GPL-3.0** (forced by NSFPlay). Recorded in `THIRD_PARTY_LICENSES.md`.
- Vendored: **NSFPlay** cores (the `xgm::` chips + bus/parser), **km6502** 6502 core (PDS = Public Domain Software, by Mamiya), **emu2413** (OPLL/VRC7, MIT) + **emu2149** (PSG/5B, MIT). Provenance + reuse modes are in the spec §9 and `THIRD_PARTY_LICENSES.md`.
- **ghostmoon-oss** (MIT, sibling repo `../ghostmoongpl`): the only external link — `ghostmoon_oss::dsp` + `::core` (DC blocker, limiter, param smoother, `ScaledEditor`, `Oscilloscope`). The proprietary `ghostmoon` is **never** linked.

---

*Maintenance: update this handoff at each phase boundary (or when the "what's next" or "open issues" lists shift materially). The granular running history lives in `CHANGELOG.md`; this doc is the orientation layer on top of it.*
