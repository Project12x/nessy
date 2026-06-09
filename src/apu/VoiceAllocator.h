#pragma once

// VoiceAllocator: routes MIDI notes to NES channels over a data-driven channel
// registry (see ChannelRegistry.h). N-channel; per-chip-group enable; modes
// Round-Robin / Pitch-Split / Unison. Drives sound through IVoiceSink so it is
// decoupled from NessyAPU (and unit-testable). GPL-3.0.

#include "ChannelRegistry.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nessy { class IVoiceSink; }

class VoiceAllocator {
public:
  enum class Mode { ROUND_ROBIN, PITCH_SPLIT, UNISON };

  VoiceAllocator();

  // The active channel set (defaults to the production registry). Resets voices.
  void setChannels(const nessy::ChannelDesc *channels, std::size_t count);

  void setAPU(nessy::IVoiceSink *apu) { m_apu = apu; }
  void setMode(Mode mode) { m_mode = mode; }
  Mode getMode() const { return m_mode; }

  // Per-chip-group enable. Core2A03 is always enabled.
  void setGroupEnabled(nessy::ChipGroup group, bool enabled);
  bool isGroupEnabled(nessy::ChipGroup group) const;

  // Back-compat shim used by PluginProcessor: VRC6 group toggle.
  void setVRC6Enabled(bool enabled) { setGroupEnabled(nessy::ChipGroup::VRC6, enabled); }
  bool isVRC6Enabled() const { return isGroupEnabled(nessy::ChipGroup::VRC6); }

  void setSplitPoint(int midiNote) { m_splitPoint = midiNote; }
  int getSplitPoint() const { return m_splitPoint; }

  void noteOn(int midiChannel, int noteNumber, float velocity);
  void noteOff(int midiChannel, int noteNumber);
  void allNotesOff();

  void arpNoteOn(int midiNote, float velocity);
  void arpNoteOff();

  // NES channel id currently holding noteNumber, or -1.
  int getChannelForNote(int noteNumber) const;

  // Channel id constants for UI reference (unchanged values).
  static constexpr int PULSE1 = 0;
  static constexpr int PULSE2 = 1;
  static constexpr int TRIANGLE = 2;
  static constexpr int NOISE = 3;
  static constexpr int VRC6_PULSE1 = 5;
  static constexpr int VRC6_PULSE2 = 6;
  static constexpr int VRC6_SAW = 7;

private:
  struct Voice {
    int noteNumber = -1;
    float velocity = 0.0f;
    uint32_t timestamp = 0;
  };

  // Indices below are POSITIONS into m_channels / m_voices, not channel ids.
  bool positionActive(std::size_t pos) const;             // melodic & group enabled
  int findFreePosition() const;                           // first active free
  int findOldestPosition() const;                         // active, lowest timestamp
  int findPositionForPitch(int noteNumber) const;         // tier-partitioned split
  int findFreeInTier(nessy::SplitTier tier) const;
  int findOldestInTier(nessy::SplitTier tier) const;
  void triggerPosition(int pos, int noteNumber, float velocity);

  nessy::IVoiceSink *m_apu = nullptr;
  Mode m_mode = Mode::ROUND_ROBIN;
  int m_splitPoint = 60;

  const nessy::ChannelDesc *m_channels = nullptr;
  std::size_t m_count = 0;
  std::vector<Voice> m_voices;        // parallel to m_channels
  std::array<bool, static_cast<std::size_t>(nessy::ChipGroup::Count)> m_groupEnabled{};
  uint32_t m_timestamp = 0;

  int m_arpLastNote = -1;
};
