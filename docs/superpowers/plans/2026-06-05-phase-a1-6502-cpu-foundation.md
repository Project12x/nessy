# Phase A.1 — 6502 CPU Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor the self-contained `km6502` 6502 core (public-domain) into Nessy, stand up the project's first C++ test harness, and prove the core is behavior-correct by passing the Klaus Dormann 6502 functional test — establishing the validated CPU that Phase B's NSF player will build on.

**Architecture:** `km6502` is a header-only 6502 core driven through a `K6502_Context` with `ReadByte`/`WriteByte` function-pointer callbacks and a `user` pointer (single-callback mode: `USE_INLINEMMC 0`, `USE_CALLBACK 1`, `USE_USERPOINTER 1`). Phase A.1 drives it against a flat 64 KB RAM and runs the functional-test ROM, asserting it reaches the success trap. NSFPlay's `nes_cpu` *wrapper* and its memory/IRQ cluster (`nes_mem`, `nsf2_irq`, `log_cpu`) are **deferred to Phase B**, where the NSF memory model lives. **This phase is purely additive: it does not modify any existing plugin source, so the synth cannot regress.**

**Tech Stack:** C++20, CMake + CPM (existing), Catch2 v3 (new, test-only), `km6502` (vendored, PDS).

**Provenance (reference-code-first):**
- `km6502` — `bbbradsmith/nsfplay` @ `6af5406e3325b5507bea1ae1a57c77d5efe5c7f3`, path `xgm/devices/CPU/km6502/`. License: **PDS / public domain** (`km6502.txt`: "License: PDS", author Mamiya). NSFPlay wrapper readme: "reuse this code without restriction."
- Test ROM — `Klaus2m5/6502_65C02_functional_tests`, `bin_files/6502_functional_test.bin`. License: GPL-3.0 (test-only artifact, not shipped in the plugin binary). Compatible with Nessy GPL-3.0.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/apu/nsfplay/xgm/devices/CPU/km6502/*` (create) | Vendored km6502 core (headers + `km6502.txt`) |
| `tests/CMakeLists.txt` (create) | Test target: Catch2 + CTest registration |
| `tests/cpu/Km6502Harness.h` (create) | Harness interface: 64 KB RAM + load/step/run |
| `tests/cpu/Km6502Harness.cpp` (create) | The **only** TU that includes `km6502m.h`; callbacks + context |
| `tests/cpu/test_km6502_functional.cpp` (create) | Catch2 tests (smoke, micro-program, functional ROM) |
| `tests/cpu/roms/6502_functional_test.bin` (create) | Klaus Dormann ROM (downloaded) |
| `tests/cpu/roms/README.md` (create) | ROM provenance + success-trap address |
| `CMakeLists.txt` (modify) | Add `NESSY_BUILD_TESTS` option + `enable_testing()` + `add_subdirectory(tests)` |
| `THIRD_PARTY_LICENSES.md` (modify) | Record km6502 (PDS) + test ROM (GPL-3.0, test-only) |

---

## Task 1: Test infrastructure (Catch2 + CTest) with a smoke test

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/cpu/test_km6502_functional.cpp`
- Modify: `CMakeLists.txt` (append, after the CPM include / `CPMAddPackage` is available)

- [ ] **Step 1: Add the smoke test (write the failing/placeholder test first)**

Create `tests/cpu/test_km6502_functional.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("test harness builds and runs", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 2: Create the test CMake**

Create `tests/CMakeLists.txt`:

```cmake
# Nessy test suite — Phase A.1+
CPMAddPackage("gh:catchorg/Catch2@3.5.2")
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(nessy_tests
    cpu/test_km6502_functional.cpp
)
target_compile_features(nessy_tests PRIVATE cxx_std_20)
target_link_libraries(nessy_tests PRIVATE Catch2::Catch2WithMain)

# Run tests from the executable's own dir so relative paths (e.g. roms/) resolve.
catch_discover_tests(nessy_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:nessy_tests>)
```

- [ ] **Step 3: Wire tests into the root build**

Append to `CMakeLists.txt` (at the end of the file, after the project's `CPMAddPackage`/JUCE setup so `CPMAddPackage` is defined):

```cmake
option(NESSY_BUILD_TESTS "Build the Nessy test suite" ON)
if(NESSY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4: Configure + build + run, verify pass**

Run:
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```
Expected: `nessy_tests` builds; `ctest` reports `100% tests passed` (1 test: `[smoke]`).

- [ ] **Step 5: Commit**

```
git add tests/CMakeLists.txt tests/cpu/test_km6502_functional.cpp CMakeLists.txt
git commit -m "test: stand up Catch2 + CTest harness (Phase A.1)"
```

---

## Task 2: Vendor km6502 + record provenance

**Files:**
- Create: `src/apu/nsfplay/xgm/devices/CPU/km6502/` (13 files from upstream)
- Modify: `THIRD_PARTY_LICENSES.md`

- [ ] **Step 1: Download the km6502 directory at the pinned SHA**

Run (PowerShell), which fetches every file in the upstream `km6502/` dir:
```powershell
$sha = "6af5406e3325b5507bea1ae1a57c77d5efe5c7f3"
$base = "repos/bbbradsmith/nsfplay/contents/xgm/devices/CPU/km6502"
$dst  = "src/apu/nsfplay/xgm/devices/CPU/km6502"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
foreach ($name in (gh api "$base?ref=$sha" --jq '.[].name')) {
    gh api "$base/$name`?ref=$sha" -H "Accept: application/vnd.github.raw" |
        Set-Content -NoNewline -Path "$dst/$name" -Encoding utf8
}
Get-ChildItem $dst | Select-Object Name
```
Expected files: `km6280.h km6280m.h km6502.h km6502.txt km6502cd.h km6502ct.h km6502ex.h km6502ft.h km6502m.h km6502ot.h km65c02.h km65c02m.h kmconfig.h`.

- [ ] **Step 2: Record provenance in `THIRD_PARTY_LICENSES.md`**

Add this entry under the dependencies list:

```markdown
### km6502 (6502 CPU core)
- **Used by:** NSF player CPU (Phase A.1 foundation), via `src/apu/nsfplay/xgm/devices/CPU/km6502/`
- **Source:** bbbradsmith/nsfplay @ 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3, path `xgm/devices/CPU/km6502/`
- **Author:** Mamiya
- **License:** PDS (Public Domain Software) — see `km6502/km6502.txt`. No copyleft obligation.
- **Reuse mode:** direct-copy (headers, unmodified)
```

- [ ] **Step 3: Commit**

```
git add src/apu/nsfplay/xgm/devices/CPU/km6502 THIRD_PARTY_LICENSES.md
git commit -m "vendor: km6502 6502 core (PDS) from nsfplay @6af5406 (Phase A.1)"
```

---

## Task 3: Km6502Harness + a micro-program test

**Files:**
- Create: `tests/cpu/Km6502Harness.h`
- Create: `tests/cpu/Km6502Harness.cpp`
- Modify: `tests/CMakeLists.txt` (add the harness sources + include path)
- Modify: `tests/cpu/test_km6502_functional.cpp` (add micro-program test)

- [ ] **Step 1: Confirm km6502's API names in the vendored header**

Open `src/apu/nsfplay/xgm/devices/CPU/km6502/km6502m.h` and confirm against this plan's assumptions:
- `struct K6502_Context` has fields `ReadByte`, `WriteByte` (single function pointers under `USE_INLINEMMC 0`), `user`, `PC`, `A`, `X`, `Y`, `S`, `P`.
- The execution entry is `K6502_Exec(&ctx)` (executes one instruction per call); the init/reset entry is `K6502_Init(&ctx)` / a reset request. If the names differ, adjust the harness below to match (the call shape is identical).
- `Callback` expands to `FastCall` (`__fastcall` on MSVC) — callback function signatures must use it.

- [ ] **Step 2: Write the harness interface**

Create `tests/cpu/Km6502Harness.h`:

```cpp
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Drives the vendored km6502 core against a flat 64 KB address space.
// km6502 is configured for single-callback memory access with a user pointer.
class Km6502Harness {
public:
    Km6502Harness();
    ~Km6502Harness();

    void load(uint16_t addr, const uint8_t* data, size_t len);
    void setPC(uint16_t pc);
    uint16_t pc() const;

    // Execute one instruction.
    void step();

    // Run until the PC stops advancing (a 1-instruction infinite loop = "trap"),
    // or until instrLimit instructions have executed.
    // Returns the trap PC, or 0xFFFFFFFF if instrLimit was reached first.
    uint32_t runUntilTrap(uint64_t instrLimit);

    std::array<uint8_t, 65536> ram{};

private:
    struct Impl;                  // hides km6502m.h (macro-impl header) in the .cpp
    std::unique_ptr<Impl> impl_;
};
```

- [ ] **Step 3: Write the harness implementation (the single km6502 TU)**

Create `tests/cpu/Km6502Harness.cpp`:

```cpp
// This is the ONLY translation unit that includes km6502m.h (a macro/inline
// implementation header). Configure it identically to nsfplay's nes_cpu, but
// leave decimal mode ENABLED so the functional test's decimal section runs.
#define ILLEGAL_OPCODES 1
#define USE_USERPOINTER 1
#define USE_INLINEMMC   0
#define USE_CALLBACK    1
#define External static
#include "km6502/km6502m.h"   // resolved via the include path added in Task 3 Step 6

#include "Km6502Harness.h"

struct Km6502Harness::Impl {
    K6502_Context ctx{};
};

// km6502 hands our `user` pointer back to each callback.
static Uword Callback harnessRead(void* user, Uword adr) {
    auto* h = static_cast<Km6502Harness*>(user);
    return h->ram[adr & 0xFFFF];
}
static void Callback harnessWrite(void* user, Uword adr, Uword value) {
    auto* h = static_cast<Km6502Harness*>(user);
    h->ram[adr & 0xFFFF] = static_cast<uint8_t>(value & 0xFF);
}

Km6502Harness::Km6502Harness() : impl_(std::make_unique<Impl>()) {
    impl_->ctx.ReadByte  = &harnessRead;
    impl_->ctx.WriteByte = &harnessWrite;
    impl_->ctx.user      = this;
    K6502_Init(&impl_->ctx);     // confirm name in Step 1; sets up internal tables
}

Km6502Harness::~Km6502Harness() = default;

void Km6502Harness::load(uint16_t addr, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i)
        ram[(addr + i) & 0xFFFF] = data[i];
}

void Km6502Harness::setPC(uint16_t pc)  { impl_->ctx.PC = pc; }
uint16_t Km6502Harness::pc() const      { return static_cast<uint16_t>(impl_->ctx.PC); }
void Km6502Harness::step()              { K6502_Exec(&impl_->ctx); }

uint32_t Km6502Harness::runUntilTrap(uint64_t instrLimit) {
    for (uint64_t i = 0; i < instrLimit; ++i) {
        const uint16_t before = static_cast<uint16_t>(impl_->ctx.PC);
        K6502_Exec(&impl_->ctx);
        if (static_cast<uint16_t>(impl_->ctx.PC) == before)
            return before;       // PC didn't move => trapped in a self-loop
    }
    return 0xFFFFFFFFu;
}
```

- [ ] **Step 4: Write the micro-program test**

Add to `tests/cpu/test_km6502_functional.cpp`:

```cpp
#include "Km6502Harness.h"

TEST_CASE("km6502 executes a basic program", "[cpu]") {
    Km6502Harness cpu;
    // LDA #$42 ; STA $10 ; JMP $0600 (self-loop)
    const uint8_t prog[] = { 0xA9, 0x42, 0x85, 0x10, 0x4C, 0x00, 0x06 };
    cpu.load(0x0600, prog, sizeof(prog));
    cpu.setPC(0x0600);

    const uint32_t trap = cpu.runUntilTrap(100);
    REQUIRE(trap == 0x0600);          // JMP-to-self trap
    REQUIRE(cpu.ram[0x10] == 0x42);   // STA wrote the accumulator
}
```

- [ ] **Step 5: Extend the test CMake**

Edit `tests/CMakeLists.txt` — replace the `add_executable(...)` block with:

```cmake
add_executable(nessy_tests
    cpu/Km6502Harness.cpp
    cpu/test_km6502_functional.cpp
)
target_compile_features(nessy_tests PRIVATE cxx_std_20)
target_include_directories(nessy_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/CPU
)
target_link_libraries(nessy_tests PRIVATE Catch2::Catch2WithMain)
```

- [ ] **Step 6: Build + run, verify pass**

Run:
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```
Expected: `[smoke]` and `[cpu]` both pass. If compilation fails on a field name (e.g. `user`, `ReadByte`, `K6502_Init`), correct it to the vendored header's spelling (Step 1) and rebuild.

- [ ] **Step 7: Commit**

```
git add tests/cpu/Km6502Harness.h tests/cpu/Km6502Harness.cpp tests/cpu/test_km6502_functional.cpp tests/CMakeLists.txt
git commit -m "test: km6502 harness + micro-program test (Phase A.1)"
```

---

## Task 4: Obtain the Klaus Dormann functional-test ROM

**Files:**
- Create: `tests/cpu/roms/6502_functional_test.bin`
- Create: `tests/cpu/roms/README.md`

- [ ] **Step 1: Download the prebuilt ROM**

Run (PowerShell):
```powershell
$dst = "tests/cpu/roms"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
$url = "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin"
Invoke-WebRequest -Uri $url -OutFile "$dst/6502_functional_test.bin"
(Get-Item "$dst/6502_functional_test.bin").Length    # expect 65536
```
Expected: a 65536-byte file. Record the resolved commit (`git ls-remote https://github.com/Klaus2m5/6502_65C02_functional_tests HEAD`) into the README below so the artifact is pinned.

- [ ] **Step 2: Record ROM provenance**

Create `tests/cpu/roms/README.md`:

```markdown
# 6502 test ROMs

## 6502_functional_test.bin
- Source: Klaus2m5/6502_65C02_functional_tests, `bin_files/6502_functional_test.bin`
- Pinned commit: <PASTE the HEAD sha from `git ls-remote` here>
- License: GPL-3.0 (test artifact; NOT shipped in the Nessy plugin binary)
- Load address: $0000 (image spans $0000–$FFFF)
- Entry point: $0400
- Success: executes to a self-loop ("trap") at **$3469** for this standard build.
  A trap at any other address indicates a failed sub-test (the address encodes which).
  If a differently-assembled ROM is used, read its `.lst` for the success label address.
- This build has decimal-mode tests ENABLED; the harness compiles km6502 with decimal enabled to match.
```

- [ ] **Step 3: Commit**

```
git add tests/cpu/roms/6502_functional_test.bin tests/cpu/roms/README.md
git commit -m "test: add Klaus Dormann 6502 functional-test ROM (Phase A.1)"
```

---

## Task 5: Functional-test runner (the real validation)

**Files:**
- Modify: `tests/cpu/test_km6502_functional.cpp`
- Modify: `tests/CMakeLists.txt` (copy `roms/` next to the test binary)

- [ ] **Step 1: Copy ROMs next to the test executable**

Append to `tests/CMakeLists.txt`:

```cmake
add_custom_command(TARGET nessy_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu/roms $<TARGET_FILE_DIR:nessy_tests>/roms)
```

- [ ] **Step 2: Write the functional-test case**

Add to `tests/cpu/test_km6502_functional.cpp`:

```cpp
#include <fstream>
#include <vector>

TEST_CASE("km6502 passes the Klaus Dormann functional test", "[cpu][functional]") {
    std::ifstream f("roms/6502_functional_test.bin", std::ios::binary);
    REQUIRE(f.good());                                  // ROM present next to the exe
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE(rom.size() == 65536);

    Km6502Harness cpu;
    cpu.load(0x0000, rom.data(), rom.size());
    cpu.setPC(0x0400);                                  // test entry point

    // ~96M cycles; cap instructions generously, then assert the success trap.
    const uint32_t trap = cpu.runUntilTrap(300'000'000ull);
    INFO("trapped at PC = 0x" << std::hex << trap);
    REQUIRE(trap == 0x3469);                            // success self-loop
}
```

- [ ] **Step 3: Build + run, verify pass**

Run:
```
cmake --build build --config Release --target nessy_tests
ctest --test-dir build -C Release --output-on-failure
```
Expected: all three cases pass. The functional test runs a few seconds in Release. If it traps at an address other than `0x3469`, the harness/core has a real bug — inspect the failing sub-test (the trap address maps to a test number in the `.lst`). Do **not** weaken the assertion to make it pass.

- [ ] **Step 4: Commit**

```
git add tests/cpu/test_km6502_functional.cpp tests/CMakeLists.txt
git commit -m "test: validate km6502 against Klaus Dormann functional test (Phase A.1)"
```

---

## Task 6: Confirm the plugin is unaffected + document

**Files:**
- Modify: `STATE.md`, `CHANGELOG.md`

- [ ] **Step 1: Build the plugin, confirm no regression**

Run:
```
cmake --build build --config Release --target Nessy_Standalone
```
Expected: builds green. (Phase A.1 added only vendored headers + a separate test target; no plugin source changed, so the standalone must build and behave identically.)

- [ ] **Step 2: Update docs**

In `CHANGELOG.md` under `## [Unreleased] / ### Added`, add:
```markdown
- **Test harness + validated 6502 core (NSF foundation, Phase A.1)** — Catch2 + CTest suite; vendored the public-domain `km6502` CPU core (nsfplay @6af5406) and validated it against the Klaus Dormann 6502 functional test. Foundation for the upcoming NSF player. No change to the synth.
```

In `STATE.md` under Build Status, add a row:
```markdown
| Tests (`nessy_tests`) | ✅ Catch2/CTest; km6502 passes Klaus Dormann functional test |
```

- [ ] **Step 3: Commit**

```
git add STATE.md CHANGELOG.md
git commit -m "docs: record Phase A.1 (test harness + validated km6502)"
```

---

## Self-Review (completed during authoring)

- **Spec coverage:** Implements the spec's Phase A "restore CPU / validate against a 6502 test ROM / synth unchanged" — scoped to the `km6502` core only (the `nes_cpu` wrapper + `nes_mem`/`nsf2_irq` cluster move to Phase B, where the memory model belongs). The 5 chips are a separate plan (Phase A.2).
- **Placeholder scan:** The only intentionally-deferred values are the test-ROM pinned commit (filled at download, Task 4 Step 2) and confirming km6502 field/init names against the vendored header (Task 3 Step 1) — both are concrete actions with the expected shape given, not vague directives.
- **Type consistency:** `Km6502Harness` interface (`load`/`setPC`/`pc`/`step`/`runUntilTrap`/`ram`) is used identically in Tasks 3 and 5; callback names (`harnessRead`/`harnessWrite`) and context fields (`ReadByte`/`WriteByte`/`user`/`PC`) are consistent throughout.
- **Risk:** exact `K6502_Context` init call (`K6502_Init`) and field spellings are confirmed in Task 3 Step 1 against the vendored header before the harness is built; the call *shape* is correct regardless of spelling.
