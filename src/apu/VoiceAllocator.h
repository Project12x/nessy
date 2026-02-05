#pragma once

// VoiceAllocator: Routes MIDI notes to NES APU channels
// GPL-3.0

#include <array>
#include <cstdint>

class NessyAPU;

class VoiceAllocator {
public:
  // Allocation modes
  enum class Mode {
    MONO,   // All notes to Pulse 1 only (authentic NES)
    POLY_4, // Round-robin: P1 → P2 → Tri → Noise (4-voice poly)
    SPLIT   // MIDI channel routing (Ch1=P1, Ch2=P2, Ch3=Tri, Ch10=Noise)
  };

  VoiceAllocator();

  void setAPU(NessyAPU *apu) { m_apu = apu; }
  void setMode(Mode mode) { m_mode = mode; }
  Mode getMode() const { return m_mode; }

  // Handle MIDI events
  void noteOn(int midiChannel, int noteNumber, float velocity);
  void noteOff(int midiChannel, int noteNumber);
  void allNotesOff();

  // Get which NES channel is playing a given note (-1 if none)
  int getChannelForNote(int noteNumber) const;

private:
  // Channel assignment for each NES channel (which MIDI note it's playing, -1
  // if none)
  struct Voice {
    int noteNumber = -1;
    float velocity = 0.0f;
    uint32_t timestamp = 0; // For note stealing (oldest note stolen first)
  };

  static constexpr int NUM_MELODIC_CHANNELS = 3; // Pulse1, Pulse2, Triangle
  static constexpr int NOISE_CHANNEL = 3;

  // Find best channel for a new note
  int findFreeChannel() const;
  int findOldestChannel() const;

  // Map MIDI channel to NES channel for split mode
  int midiChannelToNesChannel(int midiChannel) const;

  NessyAPU *m_apu = nullptr;
  Mode m_mode = Mode::POLY_4;

  std::array<Voice, 4> m_voices; // P1, P2, Tri, Noise
  uint32_t m_timestamp = 0;
};
