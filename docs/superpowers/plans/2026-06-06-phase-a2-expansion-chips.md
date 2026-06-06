# Phase A.2 — Expansion Chips Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Vendor the five missing NES expansion sound chips (MMC5, FDS, Namco 163, VRC7, Sunsoft 5B) into Nessy and smoke-test each — instantiate, enable, render, assert finite + non-silent output — establishing the chip set that Phase B (NSF playback) and Phase C (synth integration) will drive.

**Architecture:** Each chip is an `xgm::ISoundChip` (`SetClock`/`SetRate`/`Reset`/`Write`/`Tick`/`Render`) on the device model Nessy already uses (`device.h` is byte-identical across sources). Chips are compiled into the `nessy_tests` target ONLY — **the plugin is untouched** (Phase C wires them into `NessyAPU`). VRC7 needs the `emu2413` OPLL core; Sunsoft 5B needs the `emu2149` PSG core; both are MIT (Okazaki) and live under a new `legacy/` dir.

**Tech Stack:** C++20, CMake + CPM, Catch2 v3 (existing test target), vendored NSFPlay chips + `emu2413`/`emu2149`.

**Sources & provenance (reference-code-first):**
- **MMC5, FDS, N163, VRC7** chips + VRC7's **`emu2413`** (+ tone tables): **local** `dn-famitracker-source/` (matched to Nessy's existing `nes_apu/dmc/vrc6`; `device.h` verified identical). emu2413 license: **MIT** (Okazaki).
- **FME7 (Sunsoft 5B)** chip + **`emu2149`** (+ `emutypes.h` if its includes need it): `bbbradsmith/nsfplay` @ `6af5406e3325b5507bea1ae1a57c77d5efe5c7f3`, paths `xgm/devices/Sound/nes_fme7.{cpp,h}` and `xgm/devices/Sound/legacy/emu2149.{c,h}` (taken as a matched pair — `emu2149` is absent from the local reference). emu2149 license: **MIT** (Okazaki). NSFPlay chip terms: "reuse without restriction" (permissive).
- The NSFPlay chip cores share the same lineage/terms as Nessy's already-vendored chips; Nessy is GPL-3.0, so all are compatible.

**Key facts from grounding:**
- `device.h`/`devinfo.h` are byte-identical between local reference and Nessy → chips drop in cleanly.
- Only `nes_mmc5` includes `../CPU/nes_cpu.h` (the stub already present; its PCM read-path is dead-code, `cpu==NULL` guarded).
- `emu2413.c` and `emu2149.c` are **C** files → must be compiled with `LANGUAGE C`.
- Root `CMakeLists.txt` adds NSFPlay sources via an **explicit list** in `target_sources(Nessy PRIVATE ...)` (no glob); the plugin is NOT modified in this phase.
- `Render(INT32 b[2])` returns integer amplitudes → smoke assertions are **non-silent** (some non-zero sample) + **bounded** (no absurd magnitude), not float-finite.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/apu/nsfplay/xgm/devices/Sound/nes_mmc5.{cpp,h}` (create) | MMC5 chip (local) |
| `src/apu/nsfplay/xgm/devices/Sound/nes_fds.{cpp,h}` (create) | FDS chip (local) |
| `src/apu/nsfplay/xgm/devices/Sound/nes_n106.{cpp,h}` (create) | Namco 163 chip (local) |
| `src/apu/nsfplay/xgm/devices/Sound/nes_vrc7.{cpp,h}` (create) | VRC7 chip (local) |
| `src/apu/nsfplay/xgm/devices/Sound/nes_fme7.{cpp,h}` (create) | Sunsoft 5B chip (nsfplay @6af5406) |
| `src/apu/nsfplay/xgm/devices/Sound/legacy/emu2413.{c,h}` + tone `*.h` (create) | OPLL core for VRC7 (MIT) |
| `src/apu/nsfplay/xgm/devices/Sound/legacy/emu2149.{c,h}` (+ `emutypes.h` if needed) (create) | PSG core for 5B (MIT) |
| `tests/cpu/ChipSmoke.h` (create) | shared smoke helper: tick+render a chip, collect samples |
| `tests/cpu/test_expansion_chips.cpp` (create) | one Catch2 TEST_CASE per chip |
| `tests/CMakeLists.txt` (modify) | add the 5 chip `.cpp` + 2 emu `.c` (LANGUAGE C) + include dir |
| `THIRD_PARTY_LICENSES.md` (modify) | record chips + emu2413/emu2149 provenance |
| `CHANGELOG.md`, `STATE.md` (modify) | record Phase A.2 |

**Shared smoke helper** — used by every chip test. Create `tests/cpu/ChipSmoke.h`:
```cpp
#pragma once
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "xgm/devices/device.h"   // xgm::ISoundChip, INT32

// Tick+Render a chip for `samples` output frames at the given clock/rate,
// returning whether any non-zero sample was produced and whether all stayed bounded.
struct SmokeResult { bool nonSilent = false; bool bounded = true; };

inline SmokeResult smokeRender(xgm::ISoundChip& chip, int samples = 4000,
                               double clock = 1789772.7, double rate = 48000.0) {
    const int ticksPerSample = static_cast<int>(clock / rate);   // ~37
    SmokeResult r;
    for (int i = 0; i < samples; ++i) {
        chip.Tick(static_cast<xgm::UINT32>(ticksPerSample));
        xgm::INT32 b[2] = {0, 0};
        chip.Render(b);
        if (b[0] != 0 || b[1] != 0) r.nonSilent = true;
        if (std::abs((long long)b[0]) > (1LL << 24) ||
            std::abs((long long)b[1]) > (1LL << 24)) r.bounded = false;
    }
    return r;
}
```

---

## Task 1: Vendor + smoke-test MMC5, FDS, N163 (the three local, emu-free chips)

**Files:** create the 6 chip files (local copy), `tests/cpu/ChipSmoke.h`, `tests/cpu/test_expansion_chips.cpp`; modify `tests/CMakeLists.txt`, `THIRD_PARTY_LICENSES.md`.

- [ ] **Step 1: Copy the 3 chips from the local reference**
```powershell
$src = "dn-famitracker-source\Source\APU\nsfplay\xgm\devices\Sound"
$dst = "src\apu\nsfplay\xgm\devices\Sound"
foreach ($n in "nes_mmc5","nes_fds","nes_n106") {
    Copy-Item "$src\$n.cpp" "$dst\$n.cpp" -Force
    Copy-Item "$src\$n.h"   "$dst\$n.h"   -Force
}
Get-ChildItem $dst | Where-Object Name -match "mmc5|fds|n106"
```
Then read the top `#include` lines of each copied `.cpp`/`.h` to confirm they only pull `../device.h` and (for MMC5) `../CPU/nes_cpu.h` — both already present. If any pulls a missing header, STOP and report.

- [ ] **Step 2: Create the shared smoke helper** — write `tests/cpu/ChipSmoke.h` exactly as in the File Structure section above.

- [ ] **Step 3: Write the smoke tests (TDD — write first, expect link failure until Step 4)**
Create `tests/cpu/test_expansion_chips.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "ChipSmoke.h"
#include "xgm/devices/Sound/nes_mmc5.h"
#include "xgm/devices/Sound/nes_fds.h"
#include "xgm/devices/Sound/nes_n106.h"

TEST_CASE("MMC5 renders non-silent output", "[chips][mmc5]") {
    xgm::NES_MMC5 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.Write(0x5015, 0x03);   // enable both squares
    chip.Write(0x5000, 0xBF);   // sq1: 50% duty, vol 15, env off
    chip.Write(0x5002, 0xFD);   // sq1 freq low
    chip.Write(0x5003, 0x18);   // sq1 freq high + length reload
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}

TEST_CASE("FDS renders non-silent output", "[chips][fds]") {
    xgm::NES_FDS chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.Write(0x4089, 0x80);                 // wave write mode
    for (int i = 0; i < 64; ++i) chip.Write(0x4040 + i, i >> 1);  // sawtooth
    chip.Write(0x4089, 0x00);                 // wave play, master vol max
    chip.Write(0x4080, 0xBF);                 // vol env disabled, level high
    chip.Write(0x4082, 0x40);                 // freq low
    chip.Write(0x4083, 0x00);                 // freq high, clear halt
    chip.Write(0x4084, 0x80); chip.Write(0x4087, 0x80);  // mod off
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}

TEST_CASE("N163 renders non-silent output", "[chips][n163]") {
    xgm::NES_N106 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.Write(0xE000, 0x00);                 // master enable (disable bit clear)
    chip.Write(0xF800, 0x80);                 // reg ptr = 0, auto-increment
    for (int i = 0; i < 8; ++i) chip.Write(0x4800, 0x88);  // wavetable samples
    chip.Write(0xF800, 0x7F);                 // point at channel-count reg
    chip.Write(0x4800, 0x00);                 // 1 active channel
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    // NOTE: N163's register layout is intricate; if non-silent cannot be achieved
    // with a reasonable sequence, assert bounded only and report DONE_WITH_CONCERNS
    // (full correctness is validated by Phase B's real NSF playback).
    REQUIRE(r.nonSilent);
}
```

- [ ] **Step 4: Wire the chips into the test target.** In `tests/CMakeLists.txt`, after the existing `target_link_libraries(nessy_tests ...)`, add:
```cmake
target_sources(nessy_tests PRIVATE
    cpu/test_expansion_chips.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/nes_mmc5.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/nes_fds.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/nes_n106.cpp
)
target_include_directories(nessy_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/apu/nsfplay)
```
(The `src/apu/nsfplay` include dir makes `xgm/devices/Sound/...` and `xgm/devices/device.h` resolve.)

- [ ] **Step 5: Build + run.** `cmake -B build -G "Visual Studio 17 2022"`, `cmake --build build --config Release --target nessy_tests`, `ctest --test-dir build -C Release --output-on-failure`. Expected: the prior 3 tests still pass, plus `[mmc5]`, `[fds]`, `[n163]`. If a chip is silent, debug the enabling sequence against the chip's `.cpp` (and `dn-famitracker-source/Source/Channels{MMC5,FDS,N163}.cpp` for real register usage). Do not weaken `bounded`; only relax `nonSilent`→DONE_WITH_CONCERNS for N163 if genuinely intractable.

- [ ] **Step 6: Record provenance.** Append to `THIRD_PARTY_LICENSES.md` (don't disturb existing entries):
```markdown
### NSFPlay expansion chips (MMC5, FDS, Namco 163)
- **Used by:** Phase A.2 chip foundation, `src/apu/nsfplay/xgm/devices/Sound/nes_{mmc5,fds,n106}.{cpp,h}`
- **Source:** local `dn-famitracker-source/` (NSFPlay cores, same lineage as the vendored nes_apu/dmc/vrc6)
- **License:** NSFPlay cores (GPL-compatible; Nessy is GPL-3.0). Reuse mode: direct-copy.
```

- [ ] **Step 7: Commit.**
```
git add src/apu/nsfplay/xgm/devices/Sound/nes_mmc5.* src/apu/nsfplay/xgm/devices/Sound/nes_fds.* src/apu/nsfplay/xgm/devices/Sound/nes_n106.* tests/cpu/ChipSmoke.h tests/cpu/test_expansion_chips.cpp tests/CMakeLists.txt THIRD_PARTY_LICENSES.md
git commit -m "test: vendor + smoke-test MMC5/FDS/N163 expansion chips (Phase A.2)"
```
(Co-Authored-By trailer.)

---

## Task 2: Vendor + smoke-test VRC7 (+ emu2413)

**Files:** create `nes_vrc7.{cpp,h}` + `legacy/emu2413.{c,h}` + tone `*.h` (local); modify `tests/cpu/test_expansion_chips.cpp`, `tests/CMakeLists.txt`, `THIRD_PARTY_LICENSES.md`.

- [ ] **Step 1: Copy VRC7 + emu2413 from the local reference into `legacy/`.**
```powershell
$snd = "dn-famitracker-source\Source\APU\nsfplay\xgm\devices\Sound"
$dsa = "dn-famitracker-source\Source\APU\digital-sound-antiques"
$dst = "src\apu\nsfplay\xgm\devices\Sound"
New-Item -ItemType Directory -Force -Path "$dst\legacy" | Out-Null
Copy-Item "$snd\nes_vrc7.cpp" "$dst\nes_vrc7.cpp" -Force
Copy-Item "$snd\nes_vrc7.h"   "$dst\nes_vrc7.h"   -Force
foreach ($f in "emu2413.c","emu2413.h","2413tone.h","281btone.h","vrc7tone_nuke.h","vrc7tone_rw.h","vrc7tone_ft35.h","vrc7tone_ft36.h","vrc7tone_mo.h","vrc7tone_kt1.h","vrc7tone_kt2.h") {
    if (Test-Path "$dsa\$f") { Copy-Item "$dsa\$f" "$dst\legacy\$f" -Force }
}
if (Test-Path "$dsa\LICENSE") { Copy-Item "$dsa\LICENSE" "$dst\legacy\LICENSE-emu2413" -Force }
Get-ChildItem "$dst\legacy"
```
Then **read `nes_vrc7.h` and `legacy/emu2413.c`/`.h`** to confirm the exact `#include`s. `nes_vrc7.h` includes `legacy/emu2413.h`; `emu2413.c` includes the tone-table headers by name (must all be in `legacy/`). If `emu2413.h` or `emu2413.c` includes a header not copied (e.g. `emutypes.h`), copy it too from `$dsa`. If a needed file isn't in `$dsa`, STOP and report (we may need to take VRC7+emu2413 from `bbbradsmith/nsfplay@6af5406` `Sound/legacy/` instead — same as FME7 in Task 3).

- [ ] **Step 2: Add the VRC7 smoke test** to `tests/cpu/test_expansion_chips.cpp`:
```cpp
#include "xgm/devices/Sound/nes_vrc7.h"

TEST_CASE("VRC7 renders non-silent output", "[chips][vrc7]") {
    xgm::NES_VRC7 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    // ch0: instrument 1 (built-in), volume 0 (loudest); key-on at block 4
    chip.Write(0x9010, 0x30); chip.Write(0x9030, 0x10);  // reg $30: inst=1, vol=0
    chip.Write(0x9010, 0x10); chip.Write(0x9030, 0x1F);  // reg $10: F-num low
    chip.Write(0x9010, 0x20); chip.Write(0x9030, 0x24);  // reg $20: key-on, block 4, F-num hi
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}
```

- [ ] **Step 3: Wire into CMake — emu2413.c is C.** In `tests/CMakeLists.txt` add to the `target_sources(nessy_tests PRIVATE ...)` block:
```cmake
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/nes_vrc7.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/legacy/emu2413.c
```
and after the block:
```cmake
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/legacy/emu2413.c
    PROPERTIES LANGUAGE C)
```

- [ ] **Step 4: Build + run.** Same commands. Expected: `[vrc7]` passes (non-silent). If VRC7 is silent, the OPLL key-on/patch sequence needs adjusting — consult `nes_vrc7.cpp` (the `$9010/$9030` address/data protocol, `OPLL_writeReg`) and `dn-famitracker-source/Source/ChannelsVRC7.cpp`. If genuinely intractable, assert `bounded` only + DONE_WITH_CONCERNS (Phase B validates correctness).

- [ ] **Step 5: Provenance.** Append to `THIRD_PARTY_LICENSES.md`:
```markdown
### VRC7 (nes_vrc7) + emu2413 (OPLL FM core)
- **Used by:** Phase A.2, `src/apu/nsfplay/xgm/devices/Sound/nes_vrc7.{cpp,h}` + `legacy/emu2413.*`
- **VRC7 source:** local `dn-famitracker-source/` (NSFPlay core).
- **emu2413 source:** local `dn-famitracker-source/Source/APU/digital-sound-antiques/`. Author: Mitsutaka Okazaki. **License: MIT** (see `legacy/LICENSE-emu2413`). Reuse mode: direct-copy. Compiled as C.
```

- [ ] **Step 6: Commit.**
```
git add src/apu/nsfplay/xgm/devices/Sound/nes_vrc7.* src/apu/nsfplay/xgm/devices/Sound/legacy tests/cpu/test_expansion_chips.cpp tests/CMakeLists.txt THIRD_PARTY_LICENSES.md
git commit -m "test: vendor + smoke-test VRC7 + emu2413 OPLL core (Phase A.2)"
```

---

## Task 3: Vendor + smoke-test Sunsoft 5B / FME7 (+ emu2149 from pinned nsfplay)

**Files:** create `nes_fme7.{cpp,h}` + `legacy/emu2149.{c,h}` (+ `emutypes.h` if needed) from `bbbradsmith/nsfplay@6af5406`; modify `tests/cpu/test_expansion_chips.cpp`, `tests/CMakeLists.txt`, `THIRD_PARTY_LICENSES.md`.

- [ ] **Step 1: Download FME7 + emu2149 (byte-exact) from the pinned commit.**
```powershell
$sha = "6af5406e3325b5507bea1ae1a57c77d5efe5c7f3"
$base = "https://raw.githubusercontent.com/bbbradsmith/nsfplay/$sha/xgm/devices/Sound"
$dst = "src\apu\nsfplay\xgm\devices\Sound"
Invoke-WebRequest "$base/nes_fme7.cpp"      -OutFile "$dst\nes_fme7.cpp"
Invoke-WebRequest "$base/nes_fme7.h"        -OutFile "$dst\nes_fme7.h"
Invoke-WebRequest "$base/legacy/emu2149.c"  -OutFile "$dst\legacy\emu2149.c"
Invoke-WebRequest "$base/legacy/emu2149.h"  -OutFile "$dst\legacy\emu2149.h"
```
Then **read `nes_fme7.h` and `legacy/emu2149.h`** to confirm includes. `nes_fme7.h` includes `legacy/emu2149.h`; if `emu2149.h` includes `emutypes.h` (it may), download that too:
```powershell
Invoke-WebRequest "$base/legacy/emutypes.h" -OutFile "$dst\legacy\emutypes.h"
```
Verify the files are real C/C++ (not error pages). If `nes_fme7.h` (from bbbradsmith) fails to compile against Nessy's `device.h` in Step 3 due to an interface mismatch, report BLOCKED with the exact error (do not patch the vendored core blindly).

- [ ] **Step 2: Add the FME7 smoke test** to `tests/cpu/test_expansion_chips.cpp`:
```cpp
#include "xgm/devices/Sound/nes_fme7.h"

TEST_CASE("FME7 (Sunsoft 5B) renders non-silent output", "[chips][fme7]") {
    xgm::NES_FME7 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    // PSG: address via $C000, data via $E000
    chip.Write(0xC000, 0x00); chip.Write(0xE000, 0x7F);  // ch A tone period low
    chip.Write(0xC000, 0x01); chip.Write(0xE000, 0x00);  // ch A tone period high
    chip.Write(0xC000, 0x07); chip.Write(0xE000, 0x3E);  // mixer: ch A tone on, rest off
    chip.Write(0xC000, 0x08); chip.Write(0xE000, 0x0F);  // ch A amplitude = 15
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}
```

- [ ] **Step 3: Wire into CMake — emu2149.c is C.** Add to `target_sources(nessy_tests PRIVATE ...)`:
```cmake
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/nes_fme7.cpp
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/legacy/emu2149.c
```
and:
```cmake
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/Sound/legacy/emu2149.c
    PROPERTIES LANGUAGE C)
```

- [ ] **Step 4: Build + run.** Expected: `[fme7]` passes (non-silent). If silent, adjust the PSG sequence per `nes_fme7.cpp` (the `$C000`/`$E000` latch protocol). If intractable, `bounded`-only + DONE_WITH_CONCERNS.

- [ ] **Step 5: Provenance.** Append to `THIRD_PARTY_LICENSES.md`:
```markdown
### Sunsoft 5B (nes_fme7) + emu2149 (PSG core)
- **Used by:** Phase A.2, `src/apu/nsfplay/xgm/devices/Sound/nes_fme7.{cpp,h}` + `legacy/emu2149.*`
- **Source:** bbbradsmith/nsfplay @ 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (`xgm/devices/Sound/nes_fme7.*`, `legacy/emu2149.*`).
- **emu2149 author:** Mitsutaka Okazaki. **License: MIT.** NSFPlay chip: "reuse without restriction." Reuse mode: direct-copy. emu2149 compiled as C.
```

- [ ] **Step 6: Commit.**
```
git add src/apu/nsfplay/xgm/devices/Sound/nes_fme7.* src/apu/nsfplay/xgm/devices/Sound/legacy/emu2149.* src/apu/nsfplay/xgm/devices/Sound/legacy/emutypes.h tests/cpu/test_expansion_chips.cpp tests/CMakeLists.txt THIRD_PARTY_LICENSES.md
git commit -m "test: vendor + smoke-test Sunsoft 5B (FME7) + emu2149 PSG core (Phase A.2)"
```
(Stage `emutypes.h` only if it was downloaded.)

---

## Task 4: Confirm plugin unaffected + document

**Files:** modify `CHANGELOG.md`, `STATE.md`.

- [ ] **Step 1: MANDATORY build preservation, then plugin regression build.**
```powershell
$ts = Get-Date -Format "yyyy-MM-dd_HHmm"; $d = "releases\$ts"; New-Item -ItemType Directory -Force -Path $d | Out-Null
if (Test-Path "build\Nessy_artefacts\Release\Standalone\Nessy.exe") { Copy-Item "build\Nessy_artefacts\Release\Standalone\Nessy.exe" "$d\" -Force }
if (Test-Path "build\Nessy_artefacts\Release\VST3\Nessy.vst3") { Copy-Item "build\Nessy_artefacts\Release\VST3\Nessy.vst3" "$d\Nessy.vst3" -Recurse -Force }
```
Then: `cmake --build build --config Release --target Nessy_Standalone`. Phase A.2 added chips only to `nessy_tests` (not the plugin), so the plugin must build green, unchanged. If it fails, report BLOCKED (the chips must NOT have leaked into the plugin target).

- [ ] **Step 2: Update docs.** In `CHANGELOG.md` under `## [Unreleased] / ### Added`, add:
```markdown
- **Expansion chips vendored + smoke-tested (NSF foundation, Phase A.2)** — MMC5, FDS, Namco 163, VRC7 (+ MIT emu2413 OPLL), and Sunsoft 5B (+ MIT emu2149 PSG) added to the test suite and verified to render non-silent output. Not yet wired into the synth (Phase C). No change to the plugin.
```
In `STATE.md`, update the `Tests (nessy_tests)` row to mention the expansion-chip smoke tests.

- [ ] **Step 3: Commit.**
```
git add CHANGELOG.md STATE.md
git commit -m "docs: record Phase A.2 (expansion chips vendored + smoke-tested)"
```

---

## Self-Review (completed during authoring)

- **Spec coverage:** Implements the spec's "vendor + smoke-test all 5 expansion chips" (Phase A, chip half). The `nes_cpu` wrapper + NSF memory/bus remain Phase B; synth wiring remains Phase C. Chips compile into `nessy_tests` only — the plugin is untouched (regression-checked in Task 4).
- **Placeholder scan:** Register enabling sequences are concrete (from grounding); the only deferral is the explicit, reasoned fallback "if non-silent is intractable for N163/VRC7/FME7, assert bounded + DONE_WITH_CONCERNS, defer correctness to Phase B" — a documented decision, not a vague TODO. The VRC7-source fallback (local → bbbradsmith) and the optional `emutypes.h` are conditional concrete actions.
- **Type consistency:** `smokeRender`/`SmokeResult` from `ChipSmoke.h` are used identically across all chip tests; chip class names (`NES_MMC5`, `NES_FDS`, `NES_N106`, `NES_VRC7`, `NES_FME7`) and the `$C000/$E000`, `$9010/$9030`, `$5000`, `$4040`, `$4800/$F800` register protocols match the grounding report.
- **Risks:** (1) bbbradsmith `nes_fme7` vs Nessy `device.h` — verified-stable interface, but Task 3 Step 1 flags a compile mismatch as BLOCKED. (2) Non-silent sequences for FM (VRC7) and wavetable (N163) chips may need iteration against the chip source; the fallback keeps the task unblocked while preserving the `bounded` sanity assertion.
