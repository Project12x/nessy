# Third-Party Licenses

Nessy is distributed under **GPL-3.0-or-later**. The GPL is *forced* by the
NSFPlay cores it links (see below); every other distributed component is
GPL-3.0-compatible. This file lists every third-party component shipped in the
Nessy VST3 / Standalone binaries, with SPDX identifiers.

| Component | Version | SPDX-License-Identifier | Where / notes |
|---|---|---|---|
| **NSFPlay APU cores** (`xgm::`) | vendored from Dn-FamiTracker | `GPL-3.0-or-later` | `src/apu/nsfplay/` — the 2A03 + VRC6 emulation. Extracted from **Dn-FamiTracker** (a 0CC-/FamiTracker fork). **This is why the whole Nessy plugin is GPL-3.0.** ⚠️ Confirm the exact upstream Dn-FamiTracker commit/tag and pin it here. |
| **JUCE** | 8.0.4 | `AGPL-3.0-only` OR commercial | Fetched via CPM. Used under JUCE's open-source (AGPLv3) option *or* a commercial tier — AGPLv3 is GPL-3.0-compatible. ⚠️ Confirm Nessy's JUCE tier (juce.com). |
| **Blip_Buffer** | 0.4.1 | `LGPL-2.1-or-later` | `src/apu/blip_buffer/` — band-limited buffer by Shay Green (blargg). |
| **ghostmoon-oss** | sibling repo | `MIT` (+ `LGPL-2.1-or-later`) | Nessy links only the **MIT** headers: `SafetyLimiter`, `DCBlocker`, `CpuMeter`, `ParamSmoother`, `ui/Geometry`, `ui/ScaledEditor`, `ui/Oscilloscope`. The repo also contains the LGPL-2.1 `ReverbSC` family, which Nessy does **not** include. Per-file SPDX in that repo. |
| **Inter** | bundled | `OFL-1.1` | `src/resources/fonts/Inter-*.ttf` (Rasmus Andersson). |
| **Press Start 2P** | bundled | `OFL-1.1` | `src/resources/fonts/PressStart2P-Regular.ttf` (CodeMan38). |

First-party Nessy code (everything under `src/` authored for Nessy — including
the NES "Front-Loader" `NessyLookAndFeel`/`NessyScope` and `background.png`) is
**GPL-3.0-or-later**.

### km6502 (6502 CPU core)
- **Used by:** NSF player CPU (Phase A.1 foundation), via `src/apu/nsfplay/xgm/devices/CPU/km6502/`
- **Source:** bbbradsmith/nsfplay @ 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3, path `xgm/devices/CPU/km6502/`
- **Author:** Mamiya
- **License:** PDS (Public Domain Software) — see `km6502/km6502.txt`. No copyleft obligation.
- **Reuse mode:** direct-copy (headers, unmodified)

### NSFPlay expansion chips (MMC5, FDS, Namco 163)
- **Used by:** Phase A.2 chip foundation, `src/apu/nsfplay/xgm/devices/Sound/nes_{mmc5,fds,n106}.{cpp,h}`
- **Source:** local `dn-famitracker-source/` (NSFPlay cores, same lineage as the vendored nes_apu/dmc/vrc6)
- **License:** NSFPlay cores (GPL-compatible; Nessy is GPL-3.0). Reuse mode: direct-copy.

### VRC7 (nes_vrc7) + emu2413 (OPLL FM core)
- **Used by:** Phase A.2, `src/apu/nsfplay/xgm/devices/Sound/nes_vrc7.{cpp,h}` + `legacy/emu2413.*`
- **VRC7 source:** local `dn-famitracker-source/` (NSFPlay core).
- **emu2413 source:** local `dn-famitracker-source/Source/APU/digital-sound-antiques/`. Author: Mitsutaka Okazaki. **License: MIT** (see `legacy/LICENSE-emu2413`). Reuse mode: direct-copy. Compiled as C.

### Sunsoft 5B (nes_fme7) + emu2149 (PSG core)
- **Used by:** Phase A.2, `src/apu/nsfplay/xgm/devices/Sound/nes_fme7.{cpp,h}` + `legacy/emu2149.*`
- **Source:** bbbradsmith/nsfplay @ 6af5406e3325b5507bea1ae1a57c77d5efe5c7f3 (`xgm/devices/Sound/nes_fme7.*`, `legacy/emu2149.*`).
- **emu2149 author:** Mitsutaka Okazaki. **License: MIT.** NSFPlay chip: "reuse without restriction." Reuse mode: direct-copy. emu2149 compiled as C.

## Compatibility summary

NSFPlay (GPL-3.0) makes the combined work GPL-3.0. The remaining components flow
into GPL-3.0 cleanly: MIT and LGPL-2.1-or-later are GPL-3.0-compatible, OFL-1.1
fonts are data (not linked code), and JUCE's AGPLv3 option is GPL-3.0-compatible.
The previously-linked **proprietary `ghostmoon`** was removed precisely because a
proprietary dependency cannot be shipped in a GPL binary.
