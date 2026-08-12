# Nessy macOS Universal Port Design

**Date:** 2026-08-12
**Status:** Approved design; implementation pending
**Target milestone:** Working local macOS build

## 1. Goal

Port Nessy to macOS without changing its synth, NSF player, DSP, parameters,
state format, or user interface. The first milestone produces locally usable
Universal builds of the Standalone application, VST3 plugin, and Audio Unit
plugin.

The build must support both Apple Silicon and Intel slices and run on macOS 13
or newer. Development and primary validation will use an M4 Mac mini with 16 GB
RAM, macOS 26.6.1, Xcode 26.6, and Rosetta installed.

## 2. Requirements

### 2.1 Artifacts

The macOS build produces:

- `Nessy.app` (Standalone)
- `Nessy.vst3` (VST3)
- `Nessy.component` (AUv2 music device)
- `nessy_tests` (automated test executable)

Every delivered executable must contain both `arm64` and `x86_64` slices.

### 2.2 Compatibility

- Minimum supported operating system: macOS 13.
- Build system: CMake using the Xcode generator.
- Architectures: `arm64;x86_64` in one Universal build.
- Plugin identity remains unchanged: manufacturer code `Agra`, plugin code
  `Nesy`, product name `Nessy`.
- Existing APVTS parameter IDs and state serialization remain unchanged.
- Windows continues to produce VST3 and Standalone artifacts as it does now.
- The sibling `ghostmoon-oss` repository remains the shared dependency; no Mac
  fork or proprietary Ghostmoon dependency is introduced.

### 2.3 Feature parity

The macOS port preserves the current Windows feature and UI surface exactly.
It includes the current 13-channel synth, NSF/NSFe player, voice modes, macros,
arpeggiator, sweep, portamento, scopes, themes, and existing editor layout.

The port must not change chip timing, mixing, MIDI routing, real-time safety,
or audible behavior.

## 3. Chosen Approach

Use one CMake/Xcode Universal build. A checked-in Mac preset supplies the Xcode
generator, a dedicated build directory, the macOS 13 deployment target, and
the two target architectures. The plugin format list adds AU only when building
on Apple platforms; Windows retains its existing formats.

JUCE generates the platform wrappers around the same `AudioProcessor` and
editor implementation:

```text
MIDI or NSF
    -> existing Nessy processor and chip engines
    -> existing shared DSP tail
    -> JUCE Standalone, VST3, or AU wrapper
    -> macOS audio host
```

This approach avoids duplicate project files and prevents the Windows and Mac
build definitions from drifting apart.

## 4. Components and Boundaries

### 4.1 Build configuration

The root CMake configuration owns the platform-dependent format selection. The
Mac preset owns local toolchain choices rather than hard-coding Apple settings
into every build.

The Mac build uses a separate output directory so it cannot overwrite or reuse
the existing Windows build tree. For this local milestone, JUCE's
copy-after-build behavior installs the VST3 and AU into the current user's
standard plugin directories:

- `~/Library/Audio/Plug-Ins/VST3`
- `~/Library/Audio/Plug-Ins/Components`

### 4.2 JUCE wrapper layer

JUCE remains responsible for the native application and plugin shells. Nessy
does not add a separate Cocoa UI, AU-specific processor, or format-specific DSP
path.

The AU is the desktop AUv2 music-device format selected by JUCE's `AU` format,
not AUv3.

### 4.3 Portability shims

Apple-Clang fixes are allowed only where current code relies on Windows APIs,
MSVC extensions, or platform-specific library behavior. Known audit surface
includes NSF metadata character conversion on non-Windows platforms.

Each portability fix must:

1. preserve the current Windows path;
2. remain outside the real-time audio algorithm where possible;
3. avoid changing vendored chip behavior;
4. be covered by an automated test when the affected behavior is testable; and
5. be documented when it touches vendored code.

Vendored sources will not be modernized or refactored as part of the port.

### 4.4 Runtime audio path

The following remain unchanged:

- `PluginProcessor::processBlock` mode routing;
- `VoiceAllocator` and `ChannelRegistry` behavior;
- `NessyAPU` clocks, register writes, and mixing;
- `NsfEngine` loading and atomic engine-swap lifetime model;
- shared DC blocker, limiter, and master-volume tail;
- editor rendering, control bindings, and scope transport; and
- the prohibition on allocation, locking, and deletion on the audio thread.

## 5. Error Handling

Configuration failures must state the actionable cause when Xcode, the macOS
SDK, the sibling `ghostmoon-oss` checkout, or a required architecture is
unavailable.

Compilation failures are resolved at their platform boundary rather than by
disabling a format, architecture, chip core, test, or warning globally. The
build must not silently fall back to a single architecture.

Host scan failures are diagnosed by artifact type:

- confirm bundle placement and architecture slices;
- validate AU registration with `auval`;
- force a host rescan when necessary;
- inspect host logs or crash reports; and
- reproduce in a second host before attributing the failure to Nessy or a host.

## 6. Verification Plan

### 6.1 Automated build and tests

The implementation must:

1. configure the Mac preset successfully;
2. build Standalone, VST3, AU, and `nessy_tests` in Release configuration;
3. run the complete test suite natively on Apple Silicon;
4. run the Universal test executable through Rosetta as `x86_64`; and
5. inspect every delivered executable with `lipo` or an equivalent tool and
   confirm both required slices.

No test may be skipped merely because the build is running on macOS. A test
may retain an existing fixture-based skip only when its documented fixture is
absent.

### 6.2 Format validation

| Artifact | Required validation |
|---|---|
| Standalone | Launch natively and through Rosetta; select audio/MIDI devices; play and close cleanly |
| AU | Pass `auval -v aumu Nesy Agra`; scan and run in MainStage and Pedalboard3 |
| VST3 | Scan and run in REAPER, FL Studio, and Pedalboard3 |

### 6.3 Manual parity pass

At least one plugin format and the Standalone must cover the full parity pass;
the remaining hosts must cover scan, instantiation, MIDI/audio, editor open and
close, and clean unloading.

The parity pass checks:

- MIDI note-on and note-off;
- round-robin, pitch-split, and unison allocation;
- on-screen keyboard behavior;
- current UI rendering, resizing, controls, scopes, and themes;
- NSF and NSFe loading, metadata, transport, subsong navigation, and audio;
- parameter automation;
- project and state restoration; and
- clean editor closure, plugin unloading, and application shutdown.

Existing items in `TESTLATER.md` remain manual audio/UI caveats. The port must
not claim to resolve those pre-existing issues unless separately scoped and
verified.

## 7. Documentation

Implementation updates the project build and usage documentation for macOS
without removing Windows instructions. The standard project documents are
updated as appropriate, including `CHANGELOG.md`, `STATE.md`, `README.md`,
`HOWTO.md`, `ARCHITECTURE.md`, and `TESTLATER.md`.

Documentation must include the Mac configure/build commands, output locations,
local plugin install locations, host rescan guidance, AU validation command,
native and Rosetta test commands, and the deferred release steps.

## 8. Out of Scope

The first milestone excludes:

- Phase D or any other UI redesign;
- new synth or NSF-player functionality;
- DSP, mixing, timing, or parameter changes;
- resolving existing manual-test backlog items unrelated to macOS;
- signing, hardened runtime, notarization, or Apple Developer certificates;
- installer or disk-image packaging;
- automated CI or release publishing; and
- validation on physical Intel Mac hardware.

These may become later milestones after the local Universal build is stable.

## 9. Definition of Done

The local macOS port is done when all required targets build, every artifact is
Universal, automated tests pass natively and under Rosetta, AU passes `auval`,
all named hosts scan and run their applicable format, and the manual parity pass
finds no Mac-specific functional or UI regression.

Completion evidence must record the exact configure/build commands, test
results, architecture inspection, AU validation result, and host/version matrix.
