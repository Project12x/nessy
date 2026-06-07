# Phase B.2 — Plugin Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Make the working `NsfEngine` (B.1) usable from the plugin — a Synth↔NSF mode switch, RT-safe NSF file loading, the mono→stereo bridge, and a minimal load trigger — so a user can load an NSF and **hear it** in the Standalone/VST3. The full skeuomorphic NSF-mode UI is B.3.

**Architecture:** PIMPL-refactor `NsfEngine` so its header leaks no NSFPlay/km6502 symbols (clean to include alongside JUCE). The processor holds a `PlaybackMode` atomic + an active `NsfEngine`; `processBlock` branches Synth vs NSF. NSF files are parsed/built on the **message thread** and handed to the audio thread via an **atomic pointer swap** (old engine retired on a timer — no audio-thread alloc/lock/free). NSF mono int16 output is bridged to stereo float through a preallocated scratch buffer. The decorative cartridge's EJECT chip becomes the real "Load NSF…" trigger.

**Tech Stack:** C++20, CMake + CPM, JUCE 8 (`AudioProcessor`/`FileChooser`/`AudioBuffer`), the B.1 `NsfEngine`.

**Verify external JUCE APIs with Context7** before writing JUCE calls (`FileChooser` async `launchAsync`, `AudioBuffer`, `MemoryBlock`) — do not rely on memory for signatures.

**⚠️ Synth-mode-unchanged gate:** In Synth mode the existing audio path must be byte-for-byte unchanged. The processor only *adds* a mode branch + NSF members. Manual by-ear checks: (a) synth still sounds right after the wiring, (b) NSF playback works.

---

## File Structure

| File | Change |
|---|---|
| `src/nsf/NsfEngine.h` (modify) | PIMPL: replace value members with `std::unique_ptr<Impl>`; include only `<cstdint>/<memory>` + NSF metadata |
| `src/nsf/NsfEngine.cpp` (modify) | Define `struct Impl` (the NSFPlay machine); the 5 km6502 `#undef`s live here only |
| `src/PluginProcessor.h` (modify) | Add mode atomic, pending/retire/active engine pointers, scratch buffer, `loadNsf`/`retireOldEngine`/song helpers |
| `src/PluginProcessor.cpp` (modify) | `prepareToPlay` scratch alloc; `processBlock` mode branch + swap + NSF render bridge; `loadNsf` (message-thread build+publish) |
| `src/PluginEditor.h/.cpp` (modify) | Cache EJECT/arrow rects from `drawCartridge`; `mouseDown` → FileChooser + subsong; `timerCallback` retire + metadata paint |
| `CMakeLists.txt` (modify) | Add `NsfEngine.cpp` + the NSF/parser/chip/emu sources + `src/nsf` include to the `Nessy` target |
| `tests/cpu/test_nsf_bridge.cpp` (create) | Unit test the mono→stereo conversion |
| `CHANGELOG.md`, `STATE.md` (modify) | Record B.2 |

---

## Task 0: PIMPL-refactor NsfEngine (kill the macro leak)

**Prerequisite — makes `NsfEngine.h` safe to include alongside JUCE.**

**Files:** modify `src/nsf/NsfEngine.{h,cpp}`.

- [ ] **Step 1: Confirm `nsf.h` is JUCE-safe.** Verify `src/apu/nsfplay/xgm/player/nsf/nsf.h` does NOT include `nes_cpu.h`/`km6502` (it shouldn't — it's the metadata struct). If the public API needs NSF metadata (title/songs), expose it via small accessors rather than including `nsf.h` in `NsfEngine.h` if `nsf.h` turns out to pull macros; otherwise including `nsf.h` is fine.
- [ ] **Step 2: Rewrite `NsfEngine.h`** to a PIMPL: keep the public API EXACTLY (`bool load(const xgm::UINT8*, xgm::UINT32)` — or change the byte type to `const uint8_t*`/`size_t` to avoid the `xgm` type in the header; pick one and be consistent — `const uint8_t*`/`size_t` is cleanest for a JUCE-facing header; `init(int)`, `renderSamples(int16_t*, int, double)`, `unsigned cpuPC() const`, plus metadata accessors `const char* title() const`, `int songCount() const`). Header includes only `<cstdint>`, `<memory>`, `<string>`. Members: just `struct Impl; std::unique_ptr<Impl> impl_;`. Declare ctor/dtor (dtor defined in .cpp where `Impl` is complete).
- [ ] **Step 3: Move the machine into `NsfEngine.cpp`** — `struct Impl { ... all the former value members (NSF, NES_CPU, NES_MEM, NES_BANK, NSF2_Vectors, NSF2_IRQ, Bus, Layer x2, Mixer, chip unique_ptrs, amps, basecycles, clock_rest, banked) ... };`. The 5 `#undef`s (`External`/`Callback`/`Inline`/`CCall`/`FastCall`) go at the top of `NsfEngine.cpp` after the NSFPlay includes. All method bodies (`load`/`init`/`renderSamples`/`cpuPC`/metadata) operate on `impl_->...`.
- [ ] **Step 4: Build + run.** `cmake --build build --config Release --target nessy_tests`, `ctest`. The 3 NSF tests + all others must still pass (12/12). Update `tests/cpu/test_nsf_engine.cpp` only if the API byte-type changed (e.g. `load((const uint8_t*)kSyntheticNsf, sizeof(kSyntheticNsf))`).
- [ ] **Step 5: Commit** `refactor(nsf): PIMPL NsfEngine so its header is JUCE-safe (Phase B.2)`.

---

## Task 1: Wire NSF engine sources into the plugin build

**Files:** modify `CMakeLists.txt`; add `#include "nsf/NsfEngine.h"` to `PluginProcessor.h`.

- [ ] **Step 1: Add sources to the `Nessy` target.** In `CMakeLists.txt`'s `target_sources(Nessy PRIVATE ...)`, add `src/nsf/NsfEngine.cpp`, `src/apu/nsfplay/xgm/player/nsf/nsf.cpp`, and the chip/emu sources the engine links that aren't already in the plugin: `nes_mmc5.cpp`, `nes_fds.cpp`, `nes_n106.cpp`, `nes_vrc7.cpp`, `nes_fme7.cpp`, `legacy/emu2413.c`, `legacy/emu2149.c` (apply `set_source_files_properties(... LANGUAGE C)` to the two `.c`). Add `${CMAKE_CURRENT_SOURCE_DIR}/src/nsf` to `target_include_directories(Nessy ...)`.
- [ ] **Step 2: Include the engine.** Add `#include "nsf/NsfEngine.h"` to `PluginProcessor.h` (safe now that Task 0 made it macro-clean — but place it after the JUCE includes anyway).
- [ ] **Step 3: Build the plugin.** `cmake --build build --config Release --target Nessy_Standalone` — must be GREEN with no new warnings/errors and no km6502 macro C2059 errors (if any appear, Task 0's PIMPL is incomplete — fix there). No behavior change yet.
- [ ] **Step 4: Commit** `build(nsf): compile NsfEngine into the plugin (Phase B.2)`.

---

## Task 2: Processor mode infrastructure + RT-safe load + mono→stereo

**Files:** modify `src/PluginProcessor.{h,cpp}`; create `tests/cpu/test_nsf_bridge.cpp`.

- [ ] **Step 1: Add members to `PluginProcessor.h`:**
```cpp
std::atomic<int> m_playbackMode { 0 };                 // 0 = Synth, 1 = NSF
std::atomic<NsfEngine*> m_pendingNsf { nullptr };      // message thread -> audio thread
std::atomic<NsfEngine*> m_retireNsf  { nullptr };      // audio thread -> message thread (GC)
std::unique_ptr<NsfEngine> m_activeNsf;                // audio-thread-owned
std::vector<int16_t> m_nsfScratch;                     // preallocated render scratch
int m_nsfSong { 0 };
```
Plus public: `void loadNsf(const uint8_t* data, size_t size, int song = 0);` `void selectNsfSong(int song);` `void setPlaybackMode(bool nsf);` `bool isNsfMode() const;` `juce::String getNsfTitle() const; int getNsfSongCount() const;` `void retireOldEngine();` (timer-driven).

- [ ] **Step 2: `prepareToPlay`** — after the existing `apu->initialize(sampleRate)`, add: `m_nsfScratch.assign((size_t)samplesPerBlock, 0);` (resize the scratch; use `getBlockSize()`/`samplesPerBlock`). If `m_activeNsf` is non-null, `m_activeNsf->init(m_nsfSong);`.

- [ ] **Step 3: `processBlock`** — at the very top (after `ScopedNoDenormals`), add the swap + mode branch:
```cpp
// adopt a freshly-loaded engine (message thread published it)
if (auto* pending = m_pendingNsf.exchange(nullptr, std::memory_order_acquire)) {
    m_retireNsf.store(m_activeNsf.release(), std::memory_order_release); // hand old to GC
    m_activeNsf.reset(pending);
}
if (m_playbackMode.load(std::memory_order_relaxed) == 1 && m_activeNsf) {
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    if ((int)m_nsfScratch.size() < n) m_nsfScratch.assign((size_t)n, 0); // safety (shouldn't alloc in steady state)
    m_activeNsf->renderSamples(m_nsfScratch.data(), n, currentSampleRate);
    const float k = 1.0f / 32768.0f;
    for (int i = 0; i < n; ++i) { float s = m_nsfScratch[(size_t)i] * k; L[i] = s; if (R != L) R[i] = s; }
    // fall through to the SHARED tail (DC blocker / volume smoothing / safety limiter)
} else {
    // ... existing Synth path UNCHANGED (APVTS sync, MIDI, apu->process) ...
}
// shared tail: keep the existing DC blocker + volume smoothing + safety limiter applied to `buffer`
```
Restructure carefully so the **Synth branch is the existing code verbatim** and the shared DSP tail (DC/volume/limiter) runs for both. Do not allocate or lock in `processBlock`.

- [ ] **Step 4: `loadNsf` (message thread)** — builds + publishes:
```cpp
void NessyAudioProcessor::loadNsf(const uint8_t* data, size_t size, int song) {
    auto* eng = new NsfEngine();
    if (!eng->load(data, size)) { delete eng; return; }   // TODO surface error in B.3
    eng->init(song);
    m_nsfSong = song;
    if (auto* stale = m_pendingNsf.exchange(eng, std::memory_order_release)) delete stale; // not yet adopted
    m_playbackMode.store(1, std::memory_order_release);
}
void NessyAudioProcessor::retireOldEngine() { if (auto* old = m_retireNsf.exchange(nullptr)) delete old; } // call from editor timer
```
`selectNsfSong` re-loads/re-inits (simplest: rebuild via `loadNsf` from the cached bytes, or call `init(song)` on a message-thread-built engine then publish). `setPlaybackMode(false)` stores 0 and (next block) the synth path resumes — call `voiceAllocator`'s all-notes-off when leaving/entering NSF mode if needed.

- [ ] **Step 5: Mono→stereo bridge unit test.** Create `tests/cpu/test_nsf_bridge.cpp` (a free function or a tiny helper extracted from the conversion) asserting: a known int16 scratch (e.g. {32767, -32768, 0}) converts to float {~1.0, ~-1.0, 0.0} with L==R. Wire it into `nessy_tests`. (If the conversion is inline in `processBlock`, extract a `static inline void nsfMonoToStereo(const int16_t*, float*, float*, int)` into a small header so it's testable.)

- [ ] **Step 6: Build + run + commit.** Build `nessy_tests` (bridge test passes) + `Nessy_Standalone` (green). Commit `feat(nsf): processor Synth/NSF mode + RT-safe load + mono->stereo bridge (Phase B.2)`. **Milestone:** infrastructure wired; mode defaults to Synth; NSF mode renders silence until a file is loaded (Task 3).

---

## Task 3: Minimal NSF load UI (cartridge EJECT chip)

**Files:** modify `src/PluginEditor.{h,cpp}`.

- [ ] **Step 1: Cache hit-test rects.** In `PluginEditor.h` add `juce::Rectangle<int> m_ejectBounds, m_nsfPrevBounds, m_nsfNextBounds, m_cartBodyBounds;`. In `drawCartridge()` (PluginEditor.cpp), assign these from the rects it already computes for EJECT, the prev/next arrows, and the cartridge body.
- [ ] **Step 2: `mouseDown` wiring.** If the click hits `m_ejectBounds`: launch a `juce::FileChooser` (verify the async API via Context7) for `*.nsf;*.nsfe`, and in the callback read the chosen file into a `juce::MemoryBlock` and call `processorRef.loadNsf((const uint8_t*)mb.getData(), mb.getSize(), 0);`. Use a `juce::SafePointer<NessyAudioProcessorEditor>` (or capture the processor reference, which outlives the editor) to stay safe across the async callback. If the click hits `m_nsfPrevBounds`/`m_nsfNextBounds`: `processorRef.selectNsfSong(m_nsfSong ± 1)` clamped to `[0, songCount)`. If it hits `m_cartBodyBounds` while in NSF mode: `processorRef.setPlaybackMode(false)` (toggle back to Synth); a click there in Synth mode does nothing (B.3 makes this richer).
- [ ] **Step 3: `timerCallback`** — call `processorRef.retireOldEngine();` each tick (60 Hz; cheap, retires the swapped-out engine off the audio thread). When `processorRef.isNsfMode()`, refresh the cartridge body text with `getNsfTitle()` + `SONG n/m`.
- [ ] **Step 4: `drawCartridge` paint** — when NSF mode + loaded, paint the NSF title + "SONG n/M" + an "NSF" indicator in the cartridge body (replacing the static "MEGA LEAD"); otherwise the existing decorative look. Keep it minimal — B.3 does the real skin.
- [ ] **Step 5: Build + commit.** Build `Nessy_Standalone` (green). Commit `feat(nsf-ui): minimal NSF load trigger via the cartridge EJECT chip (Phase B.2)`. *(UI interaction — manual verification deferred to Task 4.)*

---

## Task 4: Polish, regression, manual verification

**Files:** modify `src/PluginProcessor.cpp` (mode-switch cleanup, state persist) + `CHANGELOG.md`/`STATE.md`.

- [ ] **Step 1: Clean mode transitions.** Entering NSF mode: ensure the synth's notes are silenced (`voiceAllocator` all-notes-off + `apu` channels). Leaving NSF mode: synth resumes cleanly (re-sync happens next block via the existing APVTS sync). Verify no stuck notes / clicks.
- [ ] **Step 2: State persistence (optional, minimal).** In `getStateInformation`, store the loaded NSF path + mode on `parameters.state` (mirroring the `uiTheme` property pattern). In `setStateInformation`, if a path is present and the file exists, reload it. Acceptable to skip if it complicates B.2 — note it for B.3.
- [ ] **Step 3: Automated regression.** `ctest` — all `nessy_tests` pass (0 synth regressions). Build preservation + `Nessy_Standalone` green.
- [ ] **Step 4: Docs.** `CHANGELOG.md` `[Unreleased]/Added`: "**NSF playback in the plugin (Phase B.2)** — Synth↔NSF mode switch, RT-safe NSF file loading (message-thread build + atomic audio-thread swap), mono→stereo bridge; load an NSF via the cartridge EJECT chip and hear it. Skeuomorphic NSF UI is B.3." Update `STATE.md`. Commit `docs: record Phase B.2 (NSF playback in the plugin)`.
- [ ] **Step 5: MANUAL verification (controller + user).** Launch the Standalone. (a) **Synth mode:** play notes — unchanged. (b) **NSF mode:** EJECT → load a real NSF (test a plain 2A03, a VRC6, and a bankswitched file) → confirm audible, correct playback; prev/next subsong; toggle back to Synth cleanly. This requires real NSF files + the user's ears — it CANNOT be fully automated.

---

## Self-Review (completed during authoring)

- **Spec coverage:** Implements the spec's B.2 (plugin mode-switch + RT-safe load) + the minimal UI to demonstrate it; defers the full skeuomorphic NSF UI + subsong/metadata polish to B.3.
- **Placeholder scan:** Concrete code for the PIMPL, the RT-swap, the mono→stereo bridge, and the processBlock branch. The few "verify the JUCE FileChooser async signature via Context7" / "confirm nsf.h is macro-clean" notes are required real verification steps, not vague TODOs. Error-surfacing on bad NSF is explicitly deferred to B.3 (a `// TODO`-marked, intentional scope cut, not a gap).
- **Type consistency:** `loadNsf`/`selectNsfSong`/`setPlaybackMode`/`isNsfMode`/`retireOldEngine` + the member names are used identically across Tasks 2–4; the `NsfEngine` public API matches Task 0's PIMPL surface.
- **Risks:** (1) **processBlock restructure** must keep the Synth path verbatim — manual by-ear check (Task 4). (2) **RT-safety** of the atomic swap — no alloc/lock/free on the audio thread (the retire-delete happens on the editor timer). (3) **The macro leak** — Task 0 PIMPL eliminates it; Task 1 build proves it. (4) NSF audio correctness in-plugin — manual check with real files (Task 4), since automated audio capture isn't set up.
