// VoiceAllocator: Routes MIDI notes to NES APU channels
// GPL-3.0

#include "VoiceAllocator.h"
#include "NessyAPU.h"

VoiceAllocator::VoiceAllocator() { allNotesOff(); }

void VoiceAllocator::noteOn(int midiChannel, int noteNumber, float velocity) {
  if (!m_apu)
    return;

  int nesChannel = -1;

  switch (m_mode) {
  case Mode::MONO:
    // Always use Pulse 1
    nesChannel = NessyAPU::PULSE1;
    break;

  case Mode::POLY_4: {
    // Check if this note is already playing
    for (int i = 0; i < NUM_MELODIC_CHANNELS; ++i) {
      if (m_voices[i].noteNumber == noteNumber) {
        nesChannel = i;
        break;
      }
    }

    // Find a free channel or steal the oldest
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

  if (nesChannel >= 0 && nesChannel < 4) {
    // Turn off any existing note on this channel
    if (m_voices[nesChannel].noteNumber >= 0) {
      m_apu->noteOff(nesChannel);
    }

    // Play the new note
    m_voices[nesChannel].noteNumber = noteNumber;
    m_voices[nesChannel].velocity = velocity;
    m_voices[nesChannel].timestamp = ++m_timestamp;

    m_apu->noteOn(nesChannel, noteNumber, velocity);
  }
}

void VoiceAllocator::noteOff(int midiChannel, int noteNumber) {
  if (!m_apu)
    return;

  // Find which channel is playing this note
  for (int i = 0; i < 4; ++i) {
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

      // In mono mode, only one note can play so we're done
      if (m_mode == Mode::MONO)
        break;
    }
  }
}

void VoiceAllocator::allNotesOff() {
  for (int i = 0; i < 4; ++i) {
    m_voices[i].noteNumber = -1;
    m_voices[i].velocity = 0.0f;
    if (m_apu)
      m_apu->noteOff(i);
  }
}

int VoiceAllocator::getChannelForNote(int noteNumber) const {
  for (int i = 0; i < 4; ++i) {
    if (m_voices[i].noteNumber == noteNumber)
      return i;
  }
  return -1;
}

int VoiceAllocator::findFreeChannel() const {
  // Priority order: Pulse 1, Pulse 2, Triangle
  for (int i = 0; i < NUM_MELODIC_CHANNELS; ++i) {
    if (m_voices[i].noteNumber < 0)
      return i;
  }
  return -1;
}

int VoiceAllocator::findOldestChannel() const {
  int oldest = 0;
  uint32_t oldestTime = m_voices[0].timestamp;

  for (int i = 1; i < NUM_MELODIC_CHANNELS; ++i) {
    if (m_voices[i].timestamp < oldestTime) {
      oldest = i;
      oldestTime = m_voices[i].timestamp;
    }
  }

  return oldest;
}

int VoiceAllocator::midiChannelToNesChannel(int midiChannel) const {
  // MIDI channels are 1-16 (or 0-15 in code)
  // Ch 1 = Pulse 1, Ch 2 = Pulse 2, Ch 3 = Triangle, Ch 10 = Noise
  switch (midiChannel) {
  case 0:
    return NessyAPU::PULSE1;
  case 1:
    return NessyAPU::PULSE2;
  case 2:
    return NessyAPU::TRIANGLE;
  case 9:
    return NessyAPU::NOISE; // Standard MIDI drum channel
  default:
    return NessyAPU::PULSE1; // Fallback
  }
}
