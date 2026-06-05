#pragma once

// MacroEngine: NES-style per-channel macro (sequence) system
// Fires at ~60Hz (one NES video frame) to write register values per channel.
// Each macro is an array of values with optional loop and release points,
// exactly as used in FamiTracker / Furnace.
// GPL-3.0

#include <array>
#include <cstdint>
#include <vector>

// Forward declaration
class NessyAPU;

// ---------------------------------------------------------------------------
// MacroSequence: a looping sequence of values (volume, arpeggio, pitch, duty)
// ---------------------------------------------------------------------------
struct MacroSequence {
  std::vector<int8_t> values;
  int loopPoint    = -1; // -1 = no loop
  int releasePoint = -1; // -1 = no release point (instant stop)

  bool empty() const { return values.empty(); }
  int  size()  const { return static_cast<int>(values.size()); }
};

// ---------------------------------------------------------------------------
// ChannelMacroSet: the four macro types for one channel
// ---------------------------------------------------------------------------
struct ChannelMacroSet {
  MacroSequence volume;    // 4-bit (0-15); not used for Triangle
  MacroSequence arpeggio;  // signed semitone offsets
  MacroSequence pitch;     // signed period offsets (in hardware units)
  MacroSequence duty;      // pulse: 0-3, VRC6 pulse: 0-7, ignored otherwise
};

// ---------------------------------------------------------------------------
// Preset IDs
// ---------------------------------------------------------------------------
enum class MacroPreset {
  NONE = 0,
  PLAIN,          // no macro (static)
  VIBRATO,        // slight pitch wobble
  VOLUME_DECAY,   // quick volume fade
  ARPEGGIO_MAJOR, // 0 / +4 / +7 cycling (major chord)
  ARPEGGIO_MINOR, // 0 / +3 / +7 cycling (minor chord)
  DUTY_SWEEP,     // duty cycle sweeps 12.5 → 50 → 25 → 12.5
  STAB,           // short attack + fast decay (SFX tone)
  COUNT
};

// ---------------------------------------------------------------------------
// MacroEngine
// ---------------------------------------------------------------------------
class MacroEngine {
public:
  // Call after constructing NessyAPU
  void setAPU(NessyAPU* apu) { m_apu = apu; }

  // Set a preset for a channel (resets state)
  void setPreset(int channel, MacroPreset preset);

  // Call from NessyAPU: note-on resets sequence positions
  void noteOn(int channel, int midiNote, float velocity);

  // note-off advances to release point if set
  void noteOff(int channel);

  // Call from NessyAPU::clockAPU every ~29830 CPU clocks (60Hz)
  void tick();

  // Silence / reset one channel
  void reset(int channel);

  // Portamento control
  void setPortamento(bool enable, float speed);

  // When standalone arpeggiator is active, skip macro arpeggio sequences
  void setArpeggiatorActive(bool active) { m_arpeggiatorActive = active; }

  static constexpr int NUM_CHANNELS = 8;

private:
  struct ChannelState {
    ChannelMacroSet macros;
    MacroPreset     preset      = MacroPreset::NONE;
    int             volPos      = 0;
    int             arpPos      = 0;
    int             pitchPos    = 0;
    int             dutyPos     = 0;
    bool            active      = false;
    bool            released    = false;
    int             baseNote    = 60;
    float           velocity    = 1.0f;
    float           portamentoOffset = 0.0f;
    int             lastNote    = -1;
  };

  NessyAPU* m_apu = nullptr;
  std::array<ChannelState, NUM_CHANNELS> m_channels;
  
  bool m_portamentoEnable = false;
  float m_portamentoSpeed = 16.0f;
  bool m_arpeggiatorActive = false;

  void applyMacroTick(int channel);
  int  advance(int& pos, const MacroSequence& seq, bool released);

  // Preset factory
  static ChannelMacroSet makePreset(MacroPreset preset);
};
