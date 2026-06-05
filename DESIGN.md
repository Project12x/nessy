# Nessy DESIGN.md — "Front-Loader"

Design system for the Nessy editor. Source: Claude Design handoff **Direction A —
"Front-Loader"** (`directionA.jsx` + `nessy.css`), recreated in JUCE + ghostmoon.
This file is the source of truth for tokens; follow it for any UI work.

## Identity

Skeuomorphic NES front-loader console. **The PCB motherboard is the visible
chassis**; grey-plastic faceplates bolt onto it (corner phillips screws). One
tactile dial (Volume); everything else is molded plastic. A whisper of CRT glow
on the scopes. Three hardware skins via a SYSTEM switch: **NES · Famicom · FDS**.

Signature element: the colored channel-bay faceplates seated on the live circuit
board, each with a CRT-glass oscilloscope.

## Artboard & scaling

- Base resolution **1040 × 508** (fixed aspect ≈ 2.047:1).
- `gm::ui::ScaledEditor(processor, 1040, 508)` (ghostmoon-oss, MIT); layout/paint always at 1040×508.
- Resizable with **locked aspect ratio** (`ComponentBoundsConstrainer::setFixedAspectRatio(1040.0/508.0)`); `setScale(width/1040)`.

## Typography

| Role | Font | Sizes |
|---|---|---|
| Wordmark / pixel labels / readouts | **Press Start 2P** | 23 (wordmark), 5–9 (labels/readouts) |
| Incidental sans | **Inter** | — |

Both already embedded in `NessyFonts` BinaryData. Use the local `getPixelFont(h)`
loader (current pattern) or `gm::Typography::getRetroFont(h)` after registering.
Pixel text is uppercase, slight letter-spacing, with a 1px dark shadow.

## Channel accent palette (shared across themes)

| Ch | Hex | | Ch | Hex |
|---|---|---|---|---|
| P1 | `#cc0000` | | DMC | `#c79a3a` |
| P2 | `#336699` | | VRC6 | `#9b59b6` |
| TRI | `#669933` | | NSE | `#cc6600` |

## Theme tokens (the SYSTEM switch swaps these)

`hdr` = header rail, `stripe` = accent line, `pad-*` = controller cluster,
`rail/chip` = cartridge rail, `face-*` = channel faceplate, `macro-bg` = macro
chip, `tray` = keyboard tray, `*-text` = pixel label inks.

| Token | NES (default) | Famicom | FDS |
|---|---|---|---|
| hdr-a / hdr-b | `#46453d` / `#302f28` | `#8f1b1b` / `#5c0f0f` | `#3c362a` / `#241f15` |
| hdr-dim | `#c2bdaf` | `#e9cba2` | `#ddcc92` |
| stripe | `#cc0000` | `#caa24a` | `#e9b81f` |
| wordmark | `#f3efe3` | `#f5edda` | `#f2cf3a` |
| subtitle | `#d96a58` | `#e2ba50` | `#e85a45` |
| subtitle text | `2A03 APU SYSTEM` | `FAMICOM · 2A03 APU` | `DISK SYSTEM · 2A03` |
| pad-shell-a / -b | `#cbc7ba` / `#b1ac9e` | `#8c1c1c` / `#560d0d` | `#2c2820` / `#161208` |
| pad-plate-a / -b | `#2c2c28` / `#151513` | `#241010` / `#120606` | `#201d16` / `#0e0c07` |
| pad-accent | `#8e2b22` | `#caa24a` | `#e9b81f` |
| pad-ab-hi / ab / lo | `#c2524a` / `#9e2420` / `#560d0a` | `#ff6a5a` / `#d11d12` / `#5e0e0e` | `#ff6a5a` / `#d11d12` / `#5e0e0e` |
| rail-a / rail-b | `#cfcbbf` / `#bdb8ab` | `#ece3cf` / `#dccfb4` | `#e8d49a` / `#d4bd72` |
| chip-a / chip-b | `#d7d3c7` / `#c3beb1` | `#ece3cf` / `#dccfb4` | `#e8d49a` / `#d4bd72` |
| face-a / face-b | `#cdc9bd` / `#bbb6a9` | `#ece3cf` / `#d8ccb2` | `#e9d6a0` / `#d6bf78` |
| face-border | `#a9a499` | `#c2b48f` | `#b3974c` |
| macro-bg | `#d9d5c9` | `#e6dcc4` | `#efe0b0` |
| name-text | `#7c7868` | `#9a7d52` | `#7a6326` |
| rail-text | `#5f5b52` | `#7a5e34` | `#6b551f` |
| tray-a / tray-b | `#4c4b43` / `#34332c` | `#6c1515` / `#3a0808` | `#3c362a` / `#241f15` |

The PCB backdrop stays its natural dark in **all** themes (no global color wash —
explicit user note). NES red, plastic greys, charcoal, ink are constants:
`--nes-red #cc0000`, `--nes-red-lit #ff2a1a`, `--ink #1c1c1a`.

## Primitives (paint recipes)

- **bevel-out** (raised plastic): top/left highlight `rgba(255,255,255,.55)` inset, bottom inner-shadow `rgba(0,0,0,.22)`, soft drop `rgba(0,0,0,.25)`.
- **bevel-in** (recessed): top inner-shadow `rgba(0,0,0,.45)`, bottom catch-light `rgba(255,255,255,.20)`.
- **LED**: radial `#ff7a6e → #cc0000(60%) → #7a0000`, bloom `rgba(255,40,26,.7)`; off = grey radial.
- **louvers** (header deck): horizontal repeating gradient, 1px light / 1px dark / 5px gap.
- **scanlines**: 1px black @ ~10% every 3px, multiply.
- **CRT glass** (scopes): corner vignette (transparent center → `rgba(0,0,0,.5)` edge), top-left diagonal sheen, phosphor scanlines `rgba(0,0,0,.30)` every 2px, trace glow.
- **PCB bg**: tile `background.png` @ 300px, color `#14161f`, with a `rgba(10,12,18,.34)` darkening layer.
- Light model: single upper-left source (matches ghostmoon standard).

## Layout (top → bottom)

1. **Header rail** — Power LED + "POWER"; NESSY wordmark + theme subtitle; spacer; controller cluster (Voice/Arp/Porta/Split); Volume dial; SYSTEM switch. Accent `stripe` along bottom edge.
2. **Cartridge preset loader** — seated cart, prev/next, EJECT/SAVE, PATCH n/06. *(Decorative shell — no preset system yet, roadmap Phase 11.)*
3. **Channel deck** on bare PCB, 4 corner screws — labels `◢ RICOH 2A03 — BASE APU` / `KONAMI VRC6 ◣`; 5 base strips (P1 P2 TRI NSE DMC) + purple divider + 3 VRC6 strips (V·P1 V·P2 SAW). Each strip: colored header tab → ON/OFF LED row → readout/duty control → MAC macro chip → CRT scope → name.
4. **Keyboard tray** — styled `MidiKeyboardComponent`, C2–C5 labels.
5. **Footer** — `v0.2.0 · GPL-3.0 · NTSC 1.789773 MHz`. Scanline overlay on top.

## Control mapping (interactive ↔ ghostmoon)

| UI element | Component | Notes |
|---|---|---|
| Volume dial | `juce::Slider` (rotary) + `NessyLookAndFeel` | the single tactile control |
| Channel ON/OFF | painted hit-tested toggle (channel LED + ON/OFF) | per-strip enable |
| Readout / duty | `juce::ComboBox` (pulse/VRC6 duty) or painted static (TRI/DMC/SAW) | duty woven in as the readout |
| Noise mode | painted hit-tested LONG/SHORT toggle | NSE readout row |
| MAC macro | `juce::ComboBox` (8 presets) | macro chip |
| Oscilloscope | `nessy::NessyScope` wrapping `gm::ui::Oscilloscope` | trace drawn in the CRT-glass window |
| Voice / Arp / Porta / Split | painted gamepad cluster (hit-tested) | header |
| Granular PATTERN/OCT/SPLIT/GLIDE | `juce::ComboBox` / `juce::Slider` + `NessyLookAndFeel` | control rail |
| Hardware sweep (P1/P2) | painted SWEEP toggle + `juce::ComboBox` (dir/rate/shf) | **woven into the P1/P2 strips** |
| SYSTEM theme switch | custom 3-segment | persisted to APVTS state property `uiTheme` |

**Theming model:** per-instance `setColour(...)` on each juce control (juce
ColourIds, read by `NessyLookAndFeel`). On theme switch: swap the active
`NessyTheme`, re-apply the `setColour` calls + scope trace colours, `repaint()`.
Custom `paint()` chrome reads from the active `NessyTheme` struct.

## Staged implementation plan

- **Pass 1 (foundation):** `ScaledEditor` 1040×508 + fixed-aspect resize; `NessyTheme` token system (all 3) + persistent SYSTEM switch; PCB chrome, header rail, corner screws, footer, scanlines; 8 channel strips (header / ON-OFF / readout-control / macro / CRT scope / name); sweep woven into P1/P2 strips; Volume dial; a functional (simplified-plastic) global cluster for Voice/Arp/Porta/Split; styled keyboard tray. NES theme fully tuned; FC/FDS selectable.
- **Pass 2:** full skeuomorphic **gamepad cluster** (D-pad, A/B buttons, Select/Start pills) replacing the simplified cluster; **cartridge preset loader** decorative shell; CRT-glass + `melatonin_blur` shadow polish; FC/FDS visual fine-tuning.
- **Pass 3:** contextual tooltip bar, `gm::AboutOverlay`, optional boot splash; wire the cartridge to a real preset system when roadmap Phase 11 lands.

Manual visual verification by the user is required after each pass (UI rule).
