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
    MONO,   // All notes to Pulse 1 only (authentic NES)
    POLY_4, // Round-robin: P1 → P2 → Tri (3-voice melodic poly)
    POLY_7, // With VRC6: P1 → P2 → Tri → VRC6_P1 → VRC6_P2 → VRC6_SAW (6-voice
            // melodic poly)
    SPLIT // MIDI channel routing (Ch1=P1, Ch2=P2, Ch3=Tri, Ch10=Noise,
          // Ch5/6/7=VRC6)
  };

  VoiceAllocator();

  void setAPU(NessyAPU *apu) { m_apu = apu; }
  void setMode(Mode mode) { m_mode = mode; }
  Mode getMode() const { return m_mode; }

  // VRC6 enable state (affects POLY_7 mode channel selection)
  void setVRC6Enabled(bool enabled) { m_vrc6Enabled = enabled; }
  bool isVRC6Enabled() const { return m_vrc6Enabled; }

  // Handle MIDI events
  void noteOn(int midiChannel, int noteNumber, float velocity);
  void noteOff(int midiChannel, int noteNumber);
  void allNotesOff();

  // Get which NES channel is playing a given note (-1 if none)
  int getChannelForNote(int noteNumber) const;

private:
  struct Voice {
    int noteNumber = -1;
    float velocity = 0.0f;
    uint32_t timestamp = 0;
  };

  // Channel counts
  static constexpr int NUM_BASE_MELODIC = 3; // Pulse1, Pulse2, Triangle
  static constexpr int NUM_VRC6_MELODIC = 3; // VRC6_P1, VRC6_P2, VRC6_SAW
  static constexpr int NUM_TOTAL_VOICES =
      8; // All channels including Noise and DMC
  static constexpr int NOISE_CHANNEL = 3;

  // Melodic channel indices for allocation
  static constexpr int MELODIC_CHANNELS_BASE[] = {0, 1, 2}; // P1, P2, Tri
  static constexpr int MELODIC_CHANNELS_VRC6[] = {
      5, 6, 7}; // VRC6_P1, VRC6_P2, VRC6_SAW

  int findFreeChannel() const;
  int findOldestChannel() const;
  int midiChannelToNesChannel(int midiChannel) const;

  NessyAPU *m_apu = nullptr;
  Mode m_mode = Mode::POLY_4;
  bool m_vrc6Enabled = false;

  std::array<Voice, NUM_TOTAL_VOICES> m_voices;
  uint32_t m_timestamp = 0;
};
