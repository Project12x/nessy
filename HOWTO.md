# HOWTO

## Prerequisites

- Windows 10/11
- Visual Studio 2022 (Community or higher) with C++ Desktop workload
- CMake 3.24+
- Git
- The **ghostmoon-oss** sibling repo checked out at `../ghostmoongpl` (relative to the nessy root)

## First-Time Build

```powershell
git clone <repo>
cd nessy
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

> **Note:** If `cmake --build` fails with "VS instance not known to installer", use MSBuild directly (see below).

## Build Standalone Only

```powershell
cmake --build build --config Release --target Nessy_Standalone
```

## Run Standalone

```powershell
.\build\Nessy_artefacts\Release\Standalone\Nessy.exe
```

## Incremental Build (MSBuild Direct)

If the CMake generator is broken (VS Installer repair needed):

```powershell
# Standalone
&"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "build\Nessy_Standalone.vcxproj" /p:Configuration=Release /m /nologo /v:minimal

# VST3
&"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "build\Nessy_VST3.vcxproj" /p:Configuration=Release /m /nologo /v:minimal
```

## Running the Test Suite

Nessy has 26 automated Catch2 tests covering the km6502 CPU, expansion chips, NsfEngine, and VoiceAllocator. There are no automated audio or UI tests — those are in TESTLATER.md.

```powershell
# Build the test target
cmake --build build --config Release --target nessy_tests

# Run all tests
ctest --test-dir build -C Release --output-on-failure
```

The Klaus Dormann 6502 functional test requires the `test-nsfs/` directory (gitignored; download NSF test files separately) and is skipped automatically when not present.

## Output Paths

| Artifact | Path |
|---|---|
| Standalone EXE | `build\Nessy_artefacts\Release\Standalone\Nessy.exe` |
| VST3 | Copied to system VST3 folder after build (`COPY_PLUGIN_AFTER_BUILD ON`) |
| Tests binary | `build\tests\Release\nessy_tests.exe` |

## Using Expansion Chips (MMC5 / Sunsoft 5B)

MMC5 and Sunsoft 5B are wired into the synth voice pool but default **disabled**. Until the Phase D multi-chip UI lands, enable them through your DAW's generic parameter view:

- `mmc5Enable` — enables MMC5 Pulse 1 & 2 (channels 8–9)
- `sunsoft5bEnable` — enables Sunsoft 5B Squares A/B/C (channels 10–12)

Macro presets are also exposed: `macroMmc5P1`, `macroMmc5P2`, `macroFme7A`, `macroFme7B`, `macroFme7C`.

## Using the NSF Player

1. Click the **EJECT** button (cartridge chip icon) on the front panel.
2. In the NSF player window, click **LOAD** and pick a `.nsf` or `.nsfe` file.
3. Metadata (title / artist / copyright / chips) populates automatically.
4. Click **PLAY**. Use **◄** / **►** to navigate subsongs.
5. Close the NSF window to return to synth mode.

The NSF player and synth are mutually exclusive — switching is RT-safe and does not lock the audio thread.

## CMakeLists.txt Key Flags

- `IS_SYNTH TRUE` — MIDI input, no audio input required
- `COPY_PLUGIN_AFTER_BUILD TRUE` — auto-deploys VST3 to system folder
- `NESSY_BUILD_TESTS ON` (default) — builds `nessy_tests` and enables CTest

## Adding New DPCM Samples

1. Convert audio to 1-bit DPCM using a tool like Furnace or FamiTracker.
2. Open `src/apu/NessyMemory.h`.
3. Add sample bytes and call `loadSample(address, data)` from `loadFactorySamples()`.
4. In `NessyAPU::noteOn` for the `DMC` case, map the MIDI note number to address/length register values.

## Fixing CMake Generator

Run from VS Developer Prompt:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022"
```

Or use the VS Installer > Repair to re-register the instance.
