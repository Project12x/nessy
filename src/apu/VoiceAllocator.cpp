// VoiceAllocator: registry-driven MIDI -> NES channel routing. GPL-3.0.

#include "VoiceAllocator.h"
#include "IVoiceSink.h"

using nessy::ChannelDesc;
using nessy::ChannelRole;
using nessy::ChipGroup;
using nessy::SplitTier;

VoiceAllocator::VoiceAllocator() {
  m_groupEnabled[static_cast<std::size_t>(ChipGroup::Core2A03)] = true;  // always on
  setChannels(nessy::kChannels.data(), nessy::kChannels.size());
}

void VoiceAllocator::setChannels(const ChannelDesc *channels, std::size_t count) {
  m_channels = channels;
  m_count = count;
  m_voices.assign(count, Voice{});
  m_timestamp = 0;
}

void VoiceAllocator::setGroupEnabled(ChipGroup group, bool enabled) {
  if (group == ChipGroup::Core2A03) return;  // Core is always enabled
  m_groupEnabled[static_cast<std::size_t>(group)] = enabled;
}

bool VoiceAllocator::isGroupEnabled(ChipGroup group) const {
  return m_groupEnabled[static_cast<std::size_t>(group)];
}

bool VoiceAllocator::positionActive(std::size_t pos) const {
  const ChannelDesc &c = m_channels[pos];
  return c.role == ChannelRole::Melodic && isGroupEnabled(c.group);
}

int VoiceAllocator::findFreePosition() const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (positionActive(i) && m_voices[i].noteNumber < 0)
      return static_cast<int>(i);
  return -1;
}

int VoiceAllocator::findOldestPosition() const {
  int oldest = -1;
  uint32_t oldestTime = 0;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (!positionActive(i)) continue;
    if (oldest < 0 || m_voices[i].timestamp < oldestTime) {
      oldest = static_cast<int>(i);
      oldestTime = m_voices[i].timestamp;
    }
  }
  return oldest;
}

int VoiceAllocator::findFreeInTier(SplitTier tier) const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (positionActive(i) && m_channels[i].splitTier == tier && m_voices[i].noteNumber < 0)
      return static_cast<int>(i);
  return -1;
}

int VoiceAllocator::findOldestInTier(SplitTier tier) const {
  int oldest = -1;
  uint32_t oldestTime = 0;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (!positionActive(i) || m_channels[i].splitTier != tier) continue;
    if (oldest < 0 || m_voices[i].timestamp < oldestTime) {
      oldest = static_cast<int>(i);
      oldestTime = m_voices[i].timestamp;
    }
  }
  return oldest;
}

int VoiceAllocator::findPositionForPitch(int noteNumber) const {
  const SplitTier tier = (noteNumber < m_splitPoint) ? SplitTier::Bass : SplitTier::Lead;
  int pos = findFreeInTier(tier);
  if (pos < 0) pos = findOldestInTier(tier);
  return pos;
}

void VoiceAllocator::triggerPosition(int pos, int noteNumber, float velocity) {
  if (pos < 0 || static_cast<std::size_t>(pos) >= m_count) return;
  if (m_voices[pos].noteNumber >= 0)
    m_apu->noteOff(m_channels[pos].id);
  m_voices[pos].noteNumber = noteNumber;
  m_voices[pos].velocity = velocity;
  m_voices[pos].timestamp = ++m_timestamp;
  m_apu->noteOn(m_channels[pos].id, noteNumber, velocity);
}

void VoiceAllocator::noteOn(int /*midiChannel*/, int noteNumber, float velocity) {
  if (!m_apu) return;

  if (m_mode == Mode::UNISON) {
    for (std::size_t i = 0; i < m_count; ++i) {
      if (!positionActive(i)) continue;
      if (!m_apu->isChannelEnabled(m_channels[i].id)) continue;  // per-channel UI enable
      triggerPosition(static_cast<int>(i), noteNumber, velocity);
    }
    return;
  }

  int pos = -1;
  if (m_mode == Mode::ROUND_ROBIN) {
    for (std::size_t i = 0; i < m_count; ++i)  // re-use a channel already on this note
      if (positionActive(i) && m_voices[i].noteNumber == noteNumber) { pos = static_cast<int>(i); break; }
    if (pos < 0) pos = findFreePosition();
    if (pos < 0) pos = findOldestPosition();
  } else { // PITCH_SPLIT
    pos = findPositionForPitch(noteNumber);
  }
  triggerPosition(pos, noteNumber, velocity);
}

void VoiceAllocator::noteOff(int /*midiChannel*/, int noteNumber) {
  if (!m_apu) return;
  for (std::size_t i = 0; i < m_count; ++i) {
    if (m_voices[i].noteNumber == noteNumber) {
      m_voices[i].noteNumber = -1;
      m_voices[i].velocity = 0.0f;
      m_apu->noteOff(m_channels[i].id);
    }
  }
}

void VoiceAllocator::allNotesOff() {
  for (std::size_t i = 0; i < m_count; ++i) {
    m_voices[i].noteNumber = -1;
    m_voices[i].velocity = 0.0f;
    if (m_apu) m_apu->noteOff(m_channels[i].id);
  }
}

int VoiceAllocator::getChannelForNote(int noteNumber) const {
  for (std::size_t i = 0; i < m_count; ++i)
    if (m_voices[i].noteNumber == noteNumber)
      return m_channels[i].id;
  return -1;
}

void VoiceAllocator::arpNoteOn(int midiNote, float velocity) {
  if (!m_apu) return;
  if (m_arpLastNote >= 0 && m_arpLastNote != midiNote)
    arpNoteOff();
  noteOn(0, midiNote, velocity);
  m_arpLastNote = midiNote;
}

void VoiceAllocator::arpNoteOff() {
  if (!m_apu || m_arpLastNote < 0) return;
  noteOff(0, m_arpLastNote);
  m_arpLastNote = -1;
}
