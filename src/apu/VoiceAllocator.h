#pragma once

// VoiceAllocator: Routes MIDI notes to NES APU channels including expansion
// chips GPL-3.0

#include <array>
#include <cstdint>

class NessyAPU;

class VoiceAllocator {
public:
  // Allocation modes
  enum class Mode {
    ROUND_ROBIN, // Cycle through channels in order
    PITCH_SPLIT, // Low notes → Triangle/Saw, High notes → Pulses
    UNISON       // Stack multiple channels on same note (fatter sound)
  };

  VoiceAllocator();

  void setAPU(NessyAPU *apu) { m_apu = apu; }
  void setMode(Mode mode) { m_mode = mode; }
  Mode getMode() const { return m_mode; }

  // VRC6 enable state (extends both modes to 6 voices)
  void setVRC6Enabled(bool enabled) { m_vrc6Enabled = enabled; }
  bool isVRC6Enabled() const { return m_vrc6Enabled; }

  // Pitch-split configuration
  void setSplitPoint(int midiNote) { m_splitPoint = midiNote; }
  int getSplitPoint() const { return m_splitPoint; }

  // Channel priority order (indices into channel arrays)
  void setChannelOrder(const std::array<int, 6> &order) {
    m_channelOrder = order;
  }
  const std::array<int, 6> &getChannelOrder() const { return m_channelOrder; }

  // Handle MIDI events
  void noteOn(int midiChannel, int noteNumber, float velocity);
  void noteOff(int midiChannel, int noteNumber);
  void allNotesOff();

  // Get which NES channel is playing a given note (-1 if none)
  int getChannelForNote(int noteNumber) const;

  // Channel indices for UI reference
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

  // Channel counts
  static constexpr int NUM_BASE_MELODIC = 3; // Pulse1, Pulse2, Triangle
  static constexpr int NUM_VRC6_MELODIC = 3; // VRC6_P1, VRC6_P2, VRC6_SAW
  static constexpr int NUM_TOTAL_VOICES = 8; // All channels including Noise/DMC

  int findFreeChannel() const;
  int findOldestChannel() const;
  int findChannelForPitch(int noteNumber) const;
  int midiChannelToNesChannel(int midiChannel) const;
  int getMaxChannels() const;

  NessyAPU *m_apu = nullptr;
  Mode m_mode = Mode::ROUND_ROBIN;
  bool m_vrc6Enabled = false;
  int m_splitPoint = 60; // C4 - notes below go to Triangle/Saw

  // Channel allocation order (default: P1, P2, Tri, VRC6_P1, VRC6_P2, VRC6_SAW)
  std::array<int, 6> m_channelOrder = {0, 1, 2, 5, 6, 7};

  std::array<Voice, NUM_TOTAL_VOICES> m_voices;
  uint32_t m_timestamp = 0;
};
