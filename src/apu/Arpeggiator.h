#pragma once

// Arpeggiator: Standalone held-note arpeggiator for NES-style synthesis.
// Ticked at 60Hz (NES frame rate). Returns next MIDI note in sequence.
// Adapted from Breadbin SID synth arpeggiator (GPL-3.0).
// No JUCE dependency — pure C++ + STL.

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>

class Arpeggiator {
public:
  enum class Pattern { Up = 0, Down, UpDown, Random };

  // Called when MIDI note is pressed/released. Rebuilds sequence.
  void noteOn(int midiNote);
  void noteOff(int midiNote);

  // Called at 60Hz. Returns next MIDI note to play, or -1 if idle.
  int tick();

  // Clear all held notes and state.
  void reset();

  void setEnabled(bool e) { enabled_ = e; }
  void setPattern(Pattern p) { pattern_ = p; rebuildSequence(); }
  void setOctaves(int oct) { octaves_ = (oct < 1) ? 1 : (oct > 4 ? 4 : oct); rebuildSequence(); }

  bool isEnabled() const { return enabled_; }
  bool hasNotes() const { return heldCount_ > 0; }
  int  getLastNote() const { return lastNote_; }

private:
  void rebuildSequence();

  // Held notes (RT-safe fixed arrays)
  std::array<int, 128> heldNotes_{};
  int heldCount_ = 0;

  // Generated sequence
  std::array<int, 512> sequence_{};
  int seqCount_ = 0;
  int seqIndex_ = 0;

  // Work buffers (avoid stack pressure per Breadbin pattern)
  std::array<int, 128> sortBuf_{};
  std::array<int, 512> baseBuf_{};

  // Config
  bool enabled_ = false;
  Pattern pattern_ = Pattern::Up;
  int octaves_ = 1;

  // State
  int lastNote_ = -1;
  std::mt19937 rng_{12345u}; // RT-safe pre-seeded RNG
};
