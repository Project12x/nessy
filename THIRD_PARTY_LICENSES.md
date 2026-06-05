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

## Compatibility summary

NSFPlay (GPL-3.0) makes the combined work GPL-3.0. The remaining components flow
into GPL-3.0 cleanly: MIT and LGPL-2.1-or-later are GPL-3.0-compatible, OFL-1.1
fonts are data (not linked code), and JUCE's AGPLv3 option is GPL-3.0-compatible.
The previously-linked **proprietary `ghostmoon`** was removed precisely because a
proprietary dependency cannot be shipped in a GPL binary.
