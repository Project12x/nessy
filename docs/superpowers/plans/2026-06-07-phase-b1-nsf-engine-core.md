# Phase B.1 — NSF Engine Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build a self-contained NSF playback engine — parse an NSF, set up the banked CPU memory, run the ripped 6502 code (INIT + 60 Hz PLAY), drive the vendored sound chips through the bus, and render audio — validated by a `nessy_tests` test that plays a synthetic first-party NSF and asserts non-silent output. No plugin mode-switch and no UI (those are B.2/B.3).

**Architecture:** Restore NSFPlay's real `xgm::NES_CPU` (km6502-driven) over the stub, plus its memory cluster (`NES_MEM` flat RAM/WRAM/image, `NES_BANK` 4 KB bankswitch, `NSF2_*`). Port NSFPlay's `NSF` parser (`Load(buf,size)` path). Write a **slim `NsfEngine`** (~200 lines) that reproduces `NSFPlayer::Reload()`'s bus topology on Nessy's `Bus`/`Layer` and runs the self-clocking `cpu.Exec` render loop — instead of vendoring the config-entangled upstream `NSFPlayer`. Validate with an embedded synthetic NSF.

**Tech Stack:** C++20, CMake + CPM, Catch2 (existing `nessy_tests`), vendored NSFPlay CPU/memory/parser cores + km6502 (already present) + the 8 chips (already present).

**Provenance (reference-code-first):** all vendored from `bbbradsmith/nsfplay` @ `6af5406e3325b5507bea1ae1a57c77d5efe5c7f3` (the pinned commit already used for km6502 + FME7). NSFPlay terms: "reuse this code without restriction" (permissive). The synthetic NSF test fixture is **first-party** (authored here — no copyright concern).

**⚠️ Key scoping note — this phase touches the plugin:** Restoring the real `nes_cpu` replaces the shared stub `nes_cpu.h`, which the synth's `nes_dmc`/`nes_mmc5` include. So the CPU-cluster `.cpp` must link into BOTH the `nessy_tests` AND the `Nessy` plugin targets, and **B1.1 must rebuild the plugin and confirm it builds + the synth is behavior-neutral** (the synth never instantiates/runs a CPU; the DMC's `cpu` pointer stays null, and `nes_dmc`/`nes_mmc5` guard CPU calls). This is the first change to the working synth's compiled code — verify carefully.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/apu/nsfplay/xgm/devices/CPU/nes_cpu.{h,cpp}` (replace .h stub; add .cpp) | Real 6502 CPU driving km6502 |
| `src/apu/nsfplay/xgm/devices/Memory/nes_mem.{h,cpp}` (create) | RAM (2 KB mirrored) + WRAM + flat $0000–$FFFF image |
| `src/apu/nsfplay/xgm/devices/Memory/nes_bank.{h,cpp}` (create) | 4 KB bankswitch ($5FF8–$5FFF → $8000–$FFFF) |
| `src/apu/nsfplay/xgm/devices/Memory/nsf2_vectors.{h,cpp}` (create) | NSF2 $FFFA–$FFFF vector overlay |
| `src/apu/nsfplay/xgm/devices/Misc/nsf2_irq.{h,cpp}` (create) | NSF2 IRQ timer ($401B–$401D) |
| `src/apu/nsfplay/xgm/devices/Misc/log_cpu.{h,cpp}` (create; .cpp is a no-op STUB) | CPU logger interface (disabled) |
| `src/apu/nsfplay/xgm/player/nsf/nsf.{h,cpp}` (create) | NSF/NSFe parser |
| `src/apu/nsfplay/xgm/player/nsf/soundData.h` (create) | `SoundData` base (parser dep) |
| `src/apu/nsfplay/xgm/devices/Audio/mixer.h`, `amplifier.h` (create) | header-only mix/amp |
| `src/nsf/NsfEngine.{h,cpp}` (create) | first-party slim engine: wiring + INIT/PLAY render loop |
| `tests/cpu/SyntheticNsf.h` (create) | first-party synthetic NSF byte array |
| `tests/cpu/test_nsf_engine.cpp` (create) | parser + INIT + non-silent render tests |
| `tests/CMakeLists.txt`, root `CMakeLists.txt` (modify) | compile the cluster + engine into both targets |
| `THIRD_PARTY_LICENSES.md`, `CHANGELOG.md`, `STATE.md` (modify) | provenance + docs |

---

## Task 1: Restore the real CPU cluster (plugin + tests)

**The riskiest task — it replaces the shared stub and touches the plugin.**

**Files:** replace `CPU/nes_cpu.h` (stub) with the real one + add `nes_cpu.cpp`; create `Memory/nes_mem.*`, `Memory/nes_bank.*`, `Memory/nsf2_vectors.*`, `Misc/nsf2_irq.*`, `Misc/log_cpu.h` + a no-op `log_cpu.cpp`. Modify both `tests/CMakeLists.txt` and root `CMakeLists.txt`.

- [ ] **Step 1: Download the cluster byte-exact from the pinned commit.**
```powershell
$sha = "6af5406e3325b5507bea1ae1a57c77d5efe5c7f3"
$ub  = "https://raw.githubusercontent.com/bbbradsmith/nsfplay/$sha/xgm/devices"
$dev = "src\apu\nsfplay\xgm\devices"
New-Item -ItemType Directory -Force -Path "$dev\Memory","$dev\Misc","$dev\Audio" | Out-Null
Invoke-WebRequest "$ub/CPU/nes_cpu.h"          -OutFile "$dev\CPU\nes_cpu.h"     # overwrites stub
Invoke-WebRequest "$ub/CPU/nes_cpu.cpp"        -OutFile "$dev\CPU\nes_cpu.cpp"
Invoke-WebRequest "$ub/Memory/nes_mem.h"       -OutFile "$dev\Memory\nes_mem.h"
Invoke-WebRequest "$ub/Memory/nes_mem.cpp"     -OutFile "$dev\Memory\nes_mem.cpp"
Invoke-WebRequest "$ub/Memory/nes_bank.h"      -OutFile "$dev\Memory\nes_bank.h"
Invoke-WebRequest "$ub/Memory/nes_bank.cpp"    -OutFile "$dev\Memory\nes_bank.cpp"
Invoke-WebRequest "$ub/Memory/nsf2_vectors.h"  -OutFile "$dev\Memory\nsf2_vectors.h"
Invoke-WebRequest "$ub/Memory/nsf2_vectors.cpp" -OutFile "$dev\Memory\nsf2_vectors.cpp"
Invoke-WebRequest "$ub/Misc/nsf2_irq.h"        -OutFile "$dev\Misc\nsf2_irq.h"
Invoke-WebRequest "$ub/Misc/nsf2_irq.cpp"      -OutFile "$dev\Misc\nsf2_irq.cpp"
Invoke-WebRequest "$ub/Misc/log_cpu.h"         -OutFile "$dev\Misc\log_cpu.h"
```
Read the `#include`s of each `.cpp`/`.h`. They should resolve against the already-present `device.h`, `xtypes.h`, `km6502/`. If `nes_cpu.cpp` / `log_cpu.h` reference anything missing beyond `log_cpu.cpp` (which you write next), download that file too — EXCEPT `player/nsf/nsf.h` and `fileutil.h` (those come via the real `log_cpu.cpp`, which you are replacing with a stub).

- [ ] **Step 2: Write a no-op `log_cpu.cpp` stub** (avoids pulling `nsf.h`/`fileutil.h` into the CPU cluster). Open the downloaded `Misc/log_cpu.h`, then create `src/apu/nsfplay/xgm/devices/Misc/log_cpu.cpp` implementing every declared method of `CPULogger` as an empty no-op (constructors do nothing; any `int`/`bool` getter returns 0/false; `log_level` defaults to 0). The CPU only uses the logger if `SetLogger(non-null)` is called — the engine never does, so no-ops are correct.

- [ ] **Step 3: Compile the cluster into the test target.** In `tests/CMakeLists.txt`, add to the `target_sources(nessy_tests PRIVATE ...)` block:
```cmake
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/CPU/nes_cpu.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Memory/nes_mem.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Memory/nes_bank.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Memory/nsf2_vectors.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Misc/nsf2_irq.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Misc/log_cpu.cpp
```

- [ ] **Step 4: Compile the cluster into the plugin** (the synth's chips now include the real `nes_cpu.h`). In root `CMakeLists.txt`, find the `target_sources(Nessy PRIVATE ...)` block that lists `nes_apu.cpp`/`nes_dmc.cpp`/`nes_vrc6.cpp` and add the same 6 `.cpp` (relative to the source dir, matching the existing entries' style):
```cmake
    src/apu/nsfplay/xgm/devices/CPU/nes_cpu.cpp
    src/apu/nsfplay/xgm/devices/Memory/nes_mem.cpp
    src/apu/nsfplay/xgm/devices/Memory/nes_bank.cpp
    src/apu/nsfplay/xgm/devices/Memory/nsf2_vectors.cpp
    src/apu/nsfplay/xgm/devices/Misc/nsf2_irq.cpp
    src/apu/nsfplay/xgm/devices/Misc/log_cpu.cpp
```

- [ ] **Step 5: Build + run tests, and build the plugin (regression).**
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```
All 8 existing tests must still pass (this proves the real `NES_CPU` is a drop-in for `nes_dmc`/`nes_mmc5`). Then **build the plugin** (preserve first):
```
$ts = Get-Date -Format "yyyy-MM-dd_HHmm"; $d="releases\$ts"; New-Item -ItemType Directory -Force -Path $d | Out-Null
if (Test-Path "build\Nessy_artefacts\Release\Standalone\Nessy.exe") { Copy-Item "build\Nessy_artefacts\Release\Standalone\Nessy.exe" "$d\" -Force }
cmake --build build --config Release --target Nessy_Standalone
```
The plugin must build GREEN. If it fails to link (undefined `NES_CPU::...`), confirm all 6 cluster `.cpp` are in the `Nessy` target. If `nes_dmc`/`nes_mmc5` won't compile against the real `nes_cpu.h`, STOP and report (the real CPU should be a superset — investigate the exact mismatch).

- [ ] **Step 6: Provenance + commit.** Append the CPU cluster to `THIRD_PARTY_LICENSES.md` (source: nsfplay @6af5406, permissive "reuse without restriction", direct-copy; `log_cpu.cpp` is a first-party no-op stub). Commit:
```
git add src/apu/nsfplay/xgm/devices/CPU src/apu/nsfplay/xgm/devices/Memory src/apu/nsfplay/xgm/devices/Misc tests/CMakeLists.txt CMakeLists.txt THIRD_PARTY_LICENSES.md
git commit -m "feat(nsf): restore real nes_cpu + memory/IRQ cluster (Phase B.1)"
```
(Co-Authored-By trailer.)

**Note for the controller:** the synth produces audio (incl. DMC drums). The CPU restoration is behavior-neutral only if the synth's DMC `cpu` pointer stays null and the guards hold — recommend a by-ear synth spot-check by the user after this task (a low-risk audio-regression check that automated tests can't cover).

---

## Task 2: Port the NSF/NSFe parser

**Files:** create `src/apu/nsfplay/xgm/player/nsf/nsf.{h,cpp}` + `soundData.h`; create `tests/cpu/SyntheticNsf.h`; modify `tests/cpu/test_nsf_engine.cpp` (new), `tests/CMakeLists.txt`.

- [ ] **Step 1: Download the parser + base** from the pinned commit:
```powershell
$sha = "6af5406e3325b5507bea1ae1a57c77d5efe5c7f3"
$up  = "https://raw.githubusercontent.com/bbbradsmith/nsfplay/$sha/xgm/player"
$dst = "src\apu\nsfplay\xgm\player\nsf"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Invoke-WebRequest "$up/nsf/nsf.h"   -OutFile "$dst\nsf.h"
Invoke-WebRequest "$up/nsf/nsf.cpp" -OutFile "$dst\nsf.cpp"
Invoke-WebRequest "$up/soundData.h" -OutFile "$dst\..\soundData.h"
```
Read `nsf.cpp`'s includes. The `Load(UINT8*, UINT32)` path must be self-contained. Guard or `#ifdef`-out the parts that pull `fileutil.h` / `pls/ppls.h` (the `LoadFile`/playlist path — not needed; tests use `Load(buf,size)`). The SJIS string conversion is already `#ifdef _WIN32`; leave it (cosmetic). If `nsf.h` includes `soundData.h` from a different relative path, adjust the placement to match.

- [ ] **Step 2: Author the synthetic NSF fixture** `tests/cpu/SyntheticNsf.h` — a first-party 128-byte header + a ~20-byte 6502 program that, on INIT, writes APU Pulse 1 (`$4000=$8F`, `$4002`/`$4003` period, `$4015=$0F`) then `RTS`; PLAY is a bare `RTS`. Encode it as `constexpr unsigned char kSyntheticNsf[] = { ... };` with `load_address=$8000`, `init_address=$8000`, `play_address=` the RTS, `songs=1`, `speed_ntsc=16639` (0x40DF), `soundchip=0`, `bankswitch[8]=0`. (Use the byte layout from the B.1 grounding report §4; assemble the 6502 by hand — it is a handful of `LDA #imm` / `STA $40xx` / `RTS` opcodes.)

- [ ] **Step 3: Parser test.** Create `tests/cpu/test_nsf_engine.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "SyntheticNsf.h"
#include "xgm/player/nsf/nsf.h"

TEST_CASE("NSF parses synthetic header", "[nsf][parse]") {
    xgm::NSF nsf;
    REQUIRE(nsf.Load(const_cast<xgm::UINT8*>(kSyntheticNsf), sizeof(kSyntheticNsf)));
    REQUIRE(nsf.load_address == 0x8000);
    REQUIRE(nsf.init_address == 0x8000);
    REQUIRE(nsf.songs == 1);
    REQUIRE(nsf.soundchip == 0);
    REQUIRE(nsf.bankswitch[0] == 0);   // non-bankswitched
}
```
(Confirm the exact field names/`Load` signature against the vendored `nsf.h`; adjust if they differ.)

- [ ] **Step 4: Wire + build + run.** Add `cpu/test_nsf_engine.cpp` + `src/apu/nsfplay/xgm/player/nsf/nsf.cpp` to `nessy_tests`. Build + ctest. Expected: `[nsf][parse]` passes (plus all prior). If `nsf.cpp` won't compile due to a missing dep, resolve per Step 1 (guard the file-reader path). Report BLOCKED if a core parse dep is unexpectedly entangled.

- [ ] **Step 5: Provenance + commit.** Append the NSF parser to `THIRD_PARTY_LICENSES.md`; note the synthetic NSF fixture is first-party. Commit `feat(nsf): port NSF/NSFe parser + synthetic test fixture (Phase B.1)`.

---

## Task 3: NsfEngine wiring (bus + bank mapper)

**Files:** create `src/apu/nsfplay/xgm/devices/Audio/{mixer.h,amplifier.h}`; create `src/nsf/NsfEngine.{h,cpp}`; modify `tests/CMakeLists.txt` (+ root `CMakeLists.txt` only if the engine should also be plugin-visible — for B.1 keep `NsfEngine` in the test target only).

- [ ] **Step 1: Vendor the header-only audio mixers.** Download `xgm/devices/Audio/mixer.h` and `amplifier.h` from the pinned commit into `src/apu/nsfplay/xgm/devices/Audio/`. Confirm they are header-only with no external deps beyond `device.h`/`xtypes.h`.

- [ ] **Step 2: Write `src/nsf/NsfEngine.h`** — the first-party engine owning the machine. Members per the B.1 grounding report §3: `xgm::NSF nsf; NES_CPU cpu; NES_MEM mem; NES_BANK bank; NSF2_Vectors nsf2_vectors; NSF2_IRQ nsf2_irq; Bus apu_bus; Layer stack, layer; Mixer mixer; NES_APU* apu; NES_DMC* dmc;` + the conditional expansion-chip pointers; plus `bool banked; double clock_rest; double basecycles = 1789773.0;`. Public API: `bool load(const xgm::UINT8* data, xgm::UINT32 size); void init(int song = 0); void renderSamples(int16_t* out, int count, double outputRate);`.

- [ ] **Step 3: Write `src/nsf/NsfEngine.cpp` `load()`** reproducing `NSFPlayer::Reload()` topology (report §3): parse via `nsf.Load`; instantiate `apu`/`dmc` (+ any expansion chip the `soundchip` byte requests); `mem.SetImage(nsf.body, nsf.load_address, nsf.bodysize)`; if any `bankswitch[i]` nonzero, set `banked=true`, `bank.SetImage(...)` + `bank.SetBankDefault(i+8, nsf.bankswitch[i])`; attach to `stack`/`layer` in the documented order (`apu_bus`→{apu,dmc}; expansion chips; then `layer`={bank?,mem}); `cpu.SetMemory(&stack)`, `cpu.SetNESMemory(&mem)`, `dmc->SetMemory(&layer)`, `cpu.SetLogger(nullptr)`. Set every chip's `SetClock(basecycles)`, `SetRate(outputRate)` is deferred to render. (`init()`/`renderSamples()` are Task 4 — leave them declared/empty here.)

- [ ] **Step 4: Compile checkpoint.** Add `NsfEngine.cpp` to `nessy_tests`; build. No runtime test yet — confirm `NsfEngine::load()` compiles + links against the real CPU cluster + chips. Commit `feat(nsf): NsfEngine machine wiring (bus + bank mapper) (Phase B.1)`.

---

## Task 4: INIT + PLAY execution loop

**Files:** `src/nsf/NsfEngine.cpp` (implement `init` + `renderSamples`); `tests/cpu/test_nsf_engine.cpp` (INIT smoke test).

- [ ] **Step 1: Implement `init(song)`** (report §3 "Reset"): `stack.Reset()`; `cpu.Reset()`; `double rate = 1000000.0 / (nsf.speed_ntsc ? nsf.speed_ntsc : 16639);`; `cpu.Start(nsf.init_address, nsf.play_address, rate, song, /*region NTSC*/0, nsf.nsf2_bits, /*enable_irq*/false, &nsf2_irq);` (match the real `Start` signature from the vendored `nes_cpu.h`). Set each active chip `SetClock(basecycles)`.

- [ ] **Step 2: Implement `renderSamples(out, count, rate)`** (report §3 render loop): set each chip `SetRate(rate)` once; then per output sample accumulate `clock_rest += basecycles / rate; int clk = (int)clock_rest; clock_rest -= clk; cpu.Exec(clk); for each active chip: chip->Tick(clk); xgm::INT32 b[2]={0,0}; mixer.Render(b);` (or sum each chip's `Render`) and write `out[i] = (int16_t) clamp(b[0])`. The CPU's self-clocking `Exec` fires PLAY internally each frame.

- [ ] **Step 3: INIT smoke test.** Add:
```cpp
#include "../../src/nsf/NsfEngine.h"   // adjust path to match include dirs

TEST_CASE("NsfEngine runs INIT", "[nsf][init]") {
    NsfEngine eng;
    REQUIRE(eng.load(kSyntheticNsf, sizeof(kSyntheticNsf)));
    eng.init(0);
    // After INIT returns, the CPU rests in the player's breaked loop ($4103).
    REQUIRE(eng.cpuPC() == 0x4103);   // add a small `unsigned cpuPC() const { return cpu.GetPC(); }` accessor
}
```
(Add the `cpuPC()` accessor to `NsfEngine`. If the rest-PC differs from `$4103` per the vendored `nes_cpu.cpp` player stub, assert the actual documented rest address.)

- [ ] **Step 4: Build + run + commit.** Expected: `[nsf][init]` passes. Commit `feat(nsf): NsfEngine INIT/PLAY execution loop (Phase B.1)`.

---

## Task 5: Non-silent render test — the B.1 milestone

**Files:** `tests/cpu/test_nsf_engine.cpp`; docs.

- [ ] **Step 1: Milestone test.**
```cpp
#include <vector>
#include <algorithm>

TEST_CASE("NsfEngine plays a synthetic NSF (non-silent)", "[nsf][render]") {
    NsfEngine eng;
    REQUIRE(eng.load(kSyntheticNsf, sizeof(kSyntheticNsf)));
    eng.init(0);
    std::vector<int16_t> buf(44100);              // ~1 second
    eng.renderSamples(buf.data(), (int)buf.size(), 44100.0);
    bool nonSilent = std::any_of(buf.begin(), buf.end(), [](int16_t s){ return s != 0; });
    REQUIRE(nonSilent);
}
```

- [ ] **Step 2: Build + run.** Expected: `[nsf][render]` passes — proving the full path: NSF parsed → CPU ran INIT → PLAY fires per frame → APU produces non-zero samples → mixer output non-silent. If silent, debug: confirm INIT actually wrote `$4015`/`$4000` (trace via `mem` image), confirm chips are `SetClock`/`SetRate`/`SetMask(0)`'d, confirm `cpu.Exec` advances. Do not weaken the assertion.

- [ ] **Step 3: Plugin regression + docs + commit.** Rebuild `Nessy_Standalone` (preserve first) — still green. Add a `CHANGELOG.md` `[Unreleased]/Added` bullet ("NSF engine core (Phase B.1) — parses NSF, runs the 6502 via restored nes_cpu, plays a synthetic NSF non-silently in tests; no UI/plugin-mode yet") and update `STATE.md`'s tests row. Commit `feat(nsf): NSF engine plays synthetic NSF (Phase B.1 milestone)` + the doc commit.

---

## Self-Review (completed during authoring)

- **Spec coverage:** Implements the spec's Phase B engine internals (`NsfFile` parser, `BankMapper`/`NesBus` via `NES_BANK`+`NES_MEM`+`Layer`, `NsfPlayer` via the slim `NsfEngine`, real `nes_cpu`). Deferred: plugin mode-switch + RT-safe load (B.2), NSF-mode UI (B.3), and full audio fidelity (RateConverter/Fader — not needed for the non-silent milestone).
- **Placeholder scan:** Vendored files are "download verbatim from the pinned commit"; the first-party code (synthetic NSF, `NsfEngine`, tests) is concrete. The few "confirm the exact signature/field/rest-PC against the vendored source" notes are real verification steps (you can't pin an upstream API you haven't vendored yet), not vague TODOs.
- **Type consistency:** `NsfEngine` API (`load`/`init`/`renderSamples`/`cpuPC`) is used identically across Tasks 3–5; chip pointers + bus members match the grounding report's topology.
- **Risks:** (1) **CPU restoration touches the plugin** — Task 1 rebuilds it + recommends a by-ear synth check (the one place automated tests can't fully cover an audio regression). (2) The slim `NsfEngine` reproduces upstream wiring by hand — the INIT-rest-PC and render-loop are validated by the `[nsf][init]` and `[nsf][render]` tests; if the synthetic NSF won't sound, the failure is concrete and debuggable (trace INIT register writes). (3) Parser dep entanglement (`fileutil`/`ppls`) — guarded out in Task 2.
