# HOWTO

## Prerequisites

- Windows 10/11
- Visual Studio 2022 (Community or higher) with C++ Desktop workload
- CMake 3.24+
- Git

## First-Time Build

```powershell
git clone <repo>
cd nessy
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

> **Note:** If `cmake --build` fails with "VS instance not known to installer", use MSBuild directly (see below).

## Incremental Build (MSBuild Direct)

If CMake generator is broken (VS Installer repair needed):

```powershell
# Standalone
&"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "build\Nessy_Standalone.vcxproj" /p:Configuration=Release /m /nologo /v:minimal

# VST3
&"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "build\Nessy_VST3.vcxproj" /p:Configuration=Release /m /nologo /v:minimal
```

## Run Standalone

```powershell
.\build\Nessy_artefacts\Release\Standalone\Nessy.exe
```

## Output Paths

| Artifact | Path |
|---|---|
| Standalone EXE | `build\Nessy_artefacts\Release\Standalone\Nessy.exe` |
| VST3 | Copied to system VST3 folder after build (COPY_PLUGIN_AFTER_BUILD ON) |

## CMakeLists.txt Key Flags

- `IS_SYNTH TRUE` — MIDI input, no audio input required
- `COPY_PLUGIN_AFTER_BUILD TRUE` — auto-deploys VST3 to system folder
- NSFPlay sources listed explicitly in `target_sources`

## Adding New DPCM Samples

1. Convert audio to 1-bit DPCM using a tool like `famitracker` or `Furnace`.
2. Open `src/apu/NessyMemory.h`.
3. Add sample bytes and call `loadSample(address, data)` from `loadFactorySamples()`.
4. In `NessyAPU::noteOn` for the `DMC` case, map note number to address/length register values.

## Fixing CMake Generator

Run from VS Developer Prompt:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022"
```
Or use the VS Installer > Repair to re-register the instance.
