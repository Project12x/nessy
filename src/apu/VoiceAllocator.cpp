// VoiceAllocator: Routes MIDI notes to NES APU channels including expansion
// chips GPL-3.0

#include "VoiceAllocator.h"
#include "NessyAPU.h"

// Static array definitions
constexpr int VoiceAllocator::MELODIC_CHANNELS_BASE[];
constexpr int VoiceAllocator::MELODIC_CHANNELS_VRC6[];

VoiceAllocator::VoiceAllocator() { allNotesOff(); }

void VoiceAllocator::noteOn(int midiChannel, int noteNumber, float velocity) {
  if (!m_apu)
    return;

  int nesChannel = -1;

  switch (m_mode) {
  case Mode::MONO:
    nesChannel = NessyAPU::PULSE1;
    break;

  case Mode::POLY_4: {
    // Check if note already playing on base channels
    for (int i = 0; i < NUM_BASE_MELODIC; ++i) {
      if (m_voices[i].noteNumber == noteNumber) {
        nesChannel = i;
        break;
      }
    }
    // Find free or steal oldest from base melodic channels
    if (nesChannel < 0) {
      nesChannel = findFreeChannel();
      if (nesChannel < 0)
        nesChannel = findOldestChannel();
    }
    break;
  }

  case Mode::POLY_7: {
    // Check all melodic channels (base + VRC6 if enabled)
    int maxChannels = m_vrc6Enabled ? (NUM_BASE_MELODIC + NUM_VRC6_MELODIC)
                                    : NUM_BASE_MELODIC;

    // Check if note already playing
    for (int i = 0; i < NUM_BASE_MELODIC; ++i) {
      if (m_voices[i].noteNumber == noteNumber) {
        nesChannel = i;
        break;
      }
    }
    if (nesChannel < 0 && m_vrc6Enabled) {
      for (int ch : MELODIC_CHANNELS_VRC6) {
        if (m_voices[ch].noteNumber == noteNumber) {
          nesChannel = ch;
          break;
        }
      }
    }

    // Find free or steal oldest
    if (nesChannel < 0) {
      nesChannel = findFreeChannel();
      if (nesChannel < 0)
        nesChannel = findOldestChannel();
    }
    break;
  }

  case Mode::SPLIT:
    nesChannel = midiChannelToNesChannel(midiChannel);
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
      // In split mode, also check MIDI channel
      if (m_mode == Mode::SPLIT) {
        int expectedChannel = midiChannelToNesChannel(midiChannel);
        if (i != expectedChannel)
          continue;
      }

      m_voices[i].noteNumber = -1;
      m_voices[i].velocity = 0.0f;
      m_apu->noteOff(i);

      if (m_mode == Mode::MONO)
        break;
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

int VoiceAllocator::findFreeChannel() const {
  // Check base melodic channels first
  for (int ch : MELODIC_CHANNELS_BASE) {
    if (m_voices[ch].noteNumber < 0)
      return ch;
  }

  // Check VRC6 channels if enabled and in POLY_7 mode
  if (m_vrc6Enabled && m_mode == Mode::POLY_7) {
    for (int ch : MELODIC_CHANNELS_VRC6) {
      if (m_voices[ch].noteNumber < 0)
        return ch;
    }
  }

  return -1;
}

int VoiceAllocator::findOldestChannel() const {
  int oldest = 0;
  uint32_t oldestTime = m_voices[0].timestamp;

  // Check base melodic channels
  for (int ch : MELODIC_CHANNELS_BASE) {
    if (m_voices[ch].timestamp < oldestTime) {
      oldest = ch;
      oldestTime = m_voices[ch].timestamp;
    }
  }

  // Check VRC6 channels if enabled and in POLY_7 mode
  if (m_vrc6Enabled && m_mode == Mode::POLY_7) {
    for (int ch : MELODIC_CHANNELS_VRC6) {
      if (m_voices[ch].timestamp < oldestTime) {
        oldest = ch;
        oldestTime = m_voices[ch].timestamp;
      }
    }
  }

  return oldest;
}

int VoiceAllocator::midiChannelToNesChannel(int midiChannel) const {
  // Extended split mode with VRC6 support
  // Ch 1 = Pulse 1, Ch 2 = Pulse 2, Ch 3 = Triangle
  // Ch 5 = VRC6_P1, Ch 6 = VRC6_P2, Ch 7 = VRC6_SAW
  // Ch 10 = Noise (standard MIDI drums)
  switch (midiChannel) {
  case 0:
    return NessyAPU::PULSE1;
  case 1:
    return NessyAPU::PULSE2;
  case 2:
    return NessyAPU::TRIANGLE;
  case 4:
    return m_vrc6Enabled ? NessyAPU::VRC6_PULSE1 : NessyAPU::PULSE1;
  case 5:
    return m_vrc6Enabled ? NessyAPU::VRC6_PULSE2 : NessyAPU::PULSE2;
  case 6:
    return m_vrc6Enabled ? NessyAPU::VRC6_SAW : NessyAPU::TRIANGLE;
  case 9:
    return NessyAPU::NOISE;
  default:
    return NessyAPU::PULSE1;
  }
}
