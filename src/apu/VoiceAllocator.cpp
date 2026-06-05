// VoiceAllocator: Routes MIDI notes to NES APU channels including expansion
// chips GPL-3.0

#include "VoiceAllocator.h"
#include "NessyAPU.h"
#include <vector>

VoiceAllocator::VoiceAllocator() { allNotesOff(); }

void VoiceAllocator::noteOn(int midiChannel, int noteNumber, float velocity) {
  if (!m_apu)
    return;

  // UNISON mode: trigger multiple channels at once
  // UNISON mode: trigger multiple channels at once
  if (m_mode == Mode::UNISON) {
    // Full stack unison: Pulse 1, Pulse 2, Triangle, VRC6 P1, VRC6 P2, VRC6 Saw
    // Excludes Noise and DMC for melodic play
    std::vector<int> unisonChannels;
    unisonChannels.push_back(NessyAPU::PULSE1);
    unisonChannels.push_back(NessyAPU::PULSE2);
    unisonChannels.push_back(NessyAPU::TRIANGLE);

    if (m_vrc6Enabled) {
      unisonChannels.push_back(NessyAPU::VRC6_PULSE1);
      unisonChannels.push_back(NessyAPU::VRC6_PULSE2);
      unisonChannels.push_back(NessyAPU::VRC6_SAW);
    }

    for (int ch : unisonChannels) {
      // Only play if channel is enabled in the UI
      if (m_apu->isChannelEnabled(ch)) {
        // Turn off existing note
        if (m_voices[ch].noteNumber >= 0) {
          m_apu->noteOff(ch);
        }
        m_voices[ch].noteNumber = noteNumber;
        m_voices[ch].velocity = velocity;
        m_voices[ch].timestamp = ++m_timestamp;
        m_apu->noteOn(ch, noteNumber, velocity);
      }
    }
    return;
  }

  // Non-unison modes: single channel allocation
  int nesChannel = -1;

  switch (m_mode) {
  case Mode::ROUND_ROBIN: {
    // Check if note already playing
    for (int i = 0; i < getMaxChannels(); ++i) {
      int ch = m_channelOrder[i];
      if (m_voices[ch].noteNumber == noteNumber) {
        nesChannel = ch;
        break;
      }
    }
    // Find free channel using priority order
    if (nesChannel < 0)
      nesChannel = findFreeChannel();
    // Steal oldest if all channels full
    if (nesChannel < 0)
      nesChannel = findOldestChannel();
    break;
  }

  case Mode::PITCH_SPLIT: {
    // Route based on pitch
    nesChannel = findChannelForPitch(noteNumber);
    break;
  }

  case Mode::UNISON:
    // Handled above
    break;
  }

  if (nesChannel >= 0 && nesChannel < NUM_TOTAL_VOICES) {
    // Turn off existing note on this channel
    if (m_voices[nesChannel].noteNumber >= 0) {
      m_apu->noteOff(nesChannel);
    }

    m_voices[nesChannel].noteNumber = noteNumber;
    m_voices[nesChannel].velocity = velocity;
    m_voices[nesChannel].timestamp = ++m_timestamp;

    m_apu->noteOn(nesChannel, noteNumber, velocity);
  }
}

void VoiceAllocator::noteOff(int midiChannel, int noteNumber) {
  if (!m_apu)
    return;

  for (int i = 0; i < NUM_TOTAL_VOICES; ++i) {
    if (m_voices[i].noteNumber == noteNumber) {
      m_voices[i].noteNumber = -1;
      m_voices[i].velocity = 0.0f;
      m_apu->noteOff(i);
    }
  }
}

void VoiceAllocator::allNotesOff() {
  for (int i = 0; i < NUM_TOTAL_VOICES; ++i) {
    m_voices[i].noteNumber = -1;
    m_voices[i].velocity = 0.0f;
    if (m_apu)
      m_apu->noteOff(i);
  }
}

int VoiceAllocator::getChannelForNote(int noteNumber) const {
  for (int i = 0; i < NUM_TOTAL_VOICES; ++i) {
    if (m_voices[i].noteNumber == noteNumber)
      return i;
  }
  return -1;
}

int VoiceAllocator::getMaxChannels() const { return m_vrc6Enabled ? 6 : 3; }

int VoiceAllocator::findFreeChannel() const {
  // Use channel order priority
  int maxCh = getMaxChannels();
  for (int i = 0; i < maxCh; ++i) {
    int ch = m_channelOrder[i];
    if (m_voices[ch].noteNumber < 0)
      return ch;
  }
  return -1;
}

int VoiceAllocator::findOldestChannel() const {
  int maxCh = getMaxChannels();
  if (maxCh == 0)
    return -1;

  int oldest = m_channelOrder[0];
  uint32_t oldestTime = m_voices[oldest].timestamp;

  for (int i = 1; i < maxCh; ++i) {
    int ch = m_channelOrder[i];
    if (m_voices[ch].timestamp < oldestTime) {
      oldest = ch;
      oldestTime = m_voices[ch].timestamp;
    }
  }

  return oldest;
}

int VoiceAllocator::findChannelForPitch(int noteNumber) const {
  // Pitch-split: low notes go to bass channels (Triangle, VRC6_SAW)
  // High notes go to pulse channels
  bool isLowNote = noteNumber < m_splitPoint;

  if (isLowNote) {
    // Bass channels: Triangle first, then VRC6_SAW if enabled
    if (m_voices[TRIANGLE].noteNumber < 0)
      return TRIANGLE;
    if (m_vrc6Enabled && m_voices[VRC6_SAW].noteNumber < 0)
      return VRC6_SAW;
    // If bass channels full, steal oldest bass channel
    if (m_vrc6Enabled) {
      return (m_voices[TRIANGLE].timestamp < m_voices[VRC6_SAW].timestamp)
                 ? TRIANGLE
                 : VRC6_SAW;
    }
    return TRIANGLE;
  } else {
    // High notes: Pulse channels (P1, P2, VRC6_P1, VRC6_P2)
    if (m_voices[PULSE1].noteNumber < 0)
      return PULSE1;
    if (m_voices[PULSE2].noteNumber < 0)
      return PULSE2;
    if (m_vrc6Enabled) {
      if (m_voices[VRC6_PULSE1].noteNumber < 0)
        return VRC6_PULSE1;
      if (m_voices[VRC6_PULSE2].noteNumber < 0)
        return VRC6_PULSE2;
    }
    // Steal oldest pulse channel
    int oldest = PULSE1;
    uint32_t oldestTime = m_voices[PULSE1].timestamp;
    if (m_voices[PULSE2].timestamp < oldestTime) {
      oldest = PULSE2;
      oldestTime = m_voices[PULSE2].timestamp;
    }
    if (m_vrc6Enabled) {
      if (m_voices[VRC6_PULSE1].timestamp < oldestTime) {
        oldest = VRC6_PULSE1;
        oldestTime = m_voices[VRC6_PULSE1].timestamp;
      }
      if (m_voices[VRC6_PULSE2].timestamp < oldestTime) {
        oldest = VRC6_PULSE2;
      }
    }
    return oldest;
  }
}

void VoiceAllocator::arpNoteOn(int midiNote, float velocity) {
  if (!m_apu)
    return;

  // Release previous arp note if different
  if (m_arpLastNote >= 0 && m_arpLastNote != midiNote) {
    arpNoteOff();
  }

  // Route through the current voice mode (reuses existing allocation logic)
  noteOn(0, midiNote, velocity);
  m_arpLastNote = midiNote;
}

void VoiceAllocator::arpNoteOff() {
  if (!m_apu || m_arpLastNote < 0)
    return;

  noteOff(0, m_arpLastNote);
  m_arpLastNote = -1;
}

int VoiceAllocator::midiChannelToNesChannel(int midiChannel) const {
  // MIDI channel to NES channel mapping for reference
  switch (midiChannel) {
  case 0:
    return PULSE1;
  case 1:
    return PULSE2;
  case 2:
    return TRIANGLE;
  case 4:
    return m_vrc6Enabled ? VRC6_PULSE1 : PULSE1;
  case 5:
    return m_vrc6Enabled ? VRC6_PULSE2 : PULSE2;
  case 6:
    return m_vrc6Enabled ? VRC6_SAW : TRIANGLE;
  case 9:
    return NOISE;
  default:
    return PULSE1;
  }
}
