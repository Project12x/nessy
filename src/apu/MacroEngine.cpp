// MacroEngine: NES-style hardware macro (sequence) system
// GPL-3.0

#include "MacroEngine.h"
#include "NessyAPU.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Preset factory
// ---------------------------------------------------------------------------
ChannelMacroSet MacroEngine::makePreset(MacroPreset preset) {
  ChannelMacroSet m;
  switch (preset) {

  case MacroPreset::PLAIN:
    // No macros — engine is a no-op
    break;

  case MacroPreset::VIBRATO: {
    // Subtle pitch wobble: sine approximation over 8 frames, looped
    // Values are period offsets in hardware units (small = subtle)
    m.pitch.values  = { 0, 1, 2, 2, 1, 0, -1, -2, -2, -1 };
    m.pitch.loopPoint = 0;
    break;
  }

  case MacroPreset::VOLUME_DECAY: {
    // Fast volume decay: 15 → 0 over 16 frames, no loop
    for (int i = 15; i >= 0; --i)
      m.volume.values.push_back(static_cast<int8_t>(i));
    m.volume.loopPoint = -1;
    break;
  }

  case MacroPreset::ARPEGGIO_MAJOR: {
    // Major chord: root / +4st / +7st, cycles at 2 frames each
    m.arpeggio.values    = { 0, 0, 4, 4, 7, 7 };
    m.arpeggio.loopPoint = 0;
    break;
  }

  case MacroPreset::ARPEGGIO_MINOR: {
    // Minor chord: root / +3st / +7st
    m.arpeggio.values    = { 0, 0, 3, 3, 7, 7 };
    m.arpeggio.loopPoint = 0;
    break;
  }

  case MacroPreset::DUTY_SWEEP: {
    // Pulse duty cycles through 12.5% → 25% → 50% → 25%, looped
    // NES: 0=12.5%, 1=25%, 2=50%, 3=75%
    m.duty.values    = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 1, 1, 1 };
    m.duty.loopPoint = 0;
    break;
  }

  case MacroPreset::STAB: {
    // Short attack + fast decay — good for SFX or stabs
    m.volume.values    = { 15, 15, 12, 9, 6, 4, 2, 1, 0 };
    m.volume.loopPoint = -1;
    // Slight pitch drop for punch
    m.pitch.values  = { 0, -1, -2, -3, -4 };
    m.pitch.loopPoint = -1;
    break;
  }

  default:
    break;
  }
  return m;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
void MacroEngine::setPreset(int channel, MacroPreset preset) {
  if (channel < 0 || channel >= NUM_CHANNELS) return;
  auto& ch     = m_channels[channel];
  ch.preset    = preset;
  ch.macros    = makePreset(preset);
  ch.volPos    = 0;
  ch.arpPos    = 0;
  ch.pitchPos  = 0;
  ch.dutyPos   = 0;
  ch.active    = false;
  ch.released  = false;
}

void MacroEngine::noteOn(int channel, int midiNote, float velocity) {
  if (channel < 0 || channel >= NUM_CHANNELS) return;
  auto& ch       = m_channels[channel];
  ch.baseNote    = midiNote;
  ch.velocity    = velocity;
  ch.volPos      = 0;
  ch.arpPos      = 0;
  ch.pitchPos    = 0;
  ch.dutyPos     = 0;
  ch.active      = true;
  ch.released    = false;
}

void MacroEngine::noteOff(int channel) {
  if (channel < 0 || channel >= NUM_CHANNELS) return;
  auto& ch    = m_channels[channel];
  ch.released = true;
  // If no release point, mark inactive immediately
  const auto& v = ch.macros.volume;
  if (!ch.active) return;
  if (v.empty() || v.releasePoint < 0)
    ch.active = false;
}

void MacroEngine::reset(int channel) {
  if (channel < 0 || channel >= NUM_CHANNELS) return;
  m_channels[channel].active   = false;
  m_channels[channel].released = false;
}

// ---------------------------------------------------------------------------
// Tick (called at 60Hz from NessyAPU)
// ---------------------------------------------------------------------------
void MacroEngine::tick() {
  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    if (m_channels[ch].active && m_channels[ch].preset != MacroPreset::NONE)
      applyMacroTick(ch);
  }
}

// advance position in a sequence, respecting loop and release points
// returns the current value (0 if sequence is empty / exhausted)
int MacroEngine::advance(int& pos, const MacroSequence& seq, bool released) {
  if (seq.empty()) return 0;

  int val = seq.values[pos];

  if (released && seq.releasePoint >= 0) {
    // jump to release segment
    if (pos < seq.releasePoint)
      pos = seq.releasePoint;
  }

  ++pos;
  if (pos >= seq.size()) {
    if (seq.loopPoint >= 0)
      pos = seq.loopPoint;
    else
      pos = seq.size() - 1; // hold last value
  }

  return val;
}

void MacroEngine::applyMacroTick(int channel) {
  if (!m_apu) return;
  auto& ch     = m_channels[channel];
  const auto& m = ch.macros;

  // --- Volume ---
  if (!m.volume.empty()) {
    int v = advance(ch.volPos, m.volume, ch.released);
    // Scale by velocity (keep it authentic: velocity maps to initial volume)
    uint8_t vol = static_cast<uint8_t>(
        std::round(v * ch.velocity));
    if (vol > 15) vol = 15;

    // Write volume register per channel type
    switch (channel) {
    case 0: /* PULSE1 */ m_apu->writeRegister(0x4000, (m_apu->getPulseDutyReg(0) << 6) | 0x30 | vol); break;
    case 1: /* PULSE2 */ m_apu->writeRegister(0x4004, (m_apu->getPulseDutyReg(1) << 6) | 0x30 | vol); break;
    case 3: /* NOISE  */ m_apu->writeRegister(0x400C, 0x30 | vol); break;
    // Triangle doesn't have volume register; VRC6 channels handled via arpeggio/duty only
    default: break;
    }
  }

  // --- Arpeggio ---
  if (!m.arpeggio.empty()) {
    int semitoneOffset = advance(ch.arpPos, m.arpeggio, ch.released);
    int note = ch.baseNote + semitoneOffset;
    note = (note < 0) ? 0 : (note > 127 ? 127 : note);
    m_apu->writeNoteRegisters(channel, note);
  }

  // --- Pitch ---
  if (!m.pitch.empty()) {
    int periodOffset = advance(ch.pitchPos, m.pitch, ch.released);
    m_apu->writePitchOffset(channel, periodOffset);
  }

  // --- Duty ---
  if (!m.duty.empty()) {
    int d = advance(ch.dutyPos, m.duty, ch.released);
    if (d < 0) d = 0;
    switch (channel) {
    case 0: /* PULSE1 */ m_apu->writeRegister(0x4000, (static_cast<uint8_t>(d) << 6) | 0x30 | 15); break;
    case 1: /* PULSE2 */ m_apu->writeRegister(0x4004, (static_cast<uint8_t>(d) << 6) | 0x30 | 15); break;
    default: break;
    }
  }
}
