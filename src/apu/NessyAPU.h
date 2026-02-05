#pragma once

// NessyAPU: Simplified NES 2A03 APU wrapper for VST use
// GPL-3.0 - Uses NSFPlay cores from Dn-FamiTracker

#include <cstdint>
#include <memory>

// Forward declarations for NSFPlay types
namespace xgm {
class NES_APU;
class NES_DMC;
} // namespace xgm
class Blip_Buffer;
template <int quality> class Blip_Synth;

class NessyAPU {
public:
  // NES APU channel indices
  enum Channel {
    PULSE1 = 0,
    PULSE2 = 1,
    TRIANGLE = 2,
    NOISE = 3,
    DMC = 4,
    NUM_CHANNELS
  };

  // Duty cycle options for pulse channels
  enum DutyCycle {
    DUTY_12_5 = 0, // 12.5%
    DUTY_25 = 1,   // 25%
    DUTY_50 = 2,   // 50%
    DUTY_75 = 3    // 75% (inverted 25%)
  };

  NessyAPU();
  ~NessyAPU();

  // Initialize with sample rate (call from prepareToPlay)
  void initialize(double sampleRate);

  // Reset APU state
  void reset();

  // Generate audio samples
  // Returns number of samples generated
  int process(float *leftOutput, float *rightOutput, int numSamples);

  // MIDI note control
  void noteOn(int channel, int midiNote, float velocity);
  void noteOff(int channel);

  // Channel configuration
  void setChannelEnabled(int channel, bool enabled);
  void setPulseDuty(int pulseChannel, DutyCycle duty); // 0=Pulse1, 1=Pulse2
  void setNoiseMode(
      bool shortMode); // false=long (32767 steps), true=short (93 steps)

  // Get channel frequency for visualization
  double getChannelFrequency(int channel) const;

  // Direct register access (for advanced use)
  void writeRegister(uint16_t address, uint8_t value);

private:
  // Convert MIDI note to NES period value
  uint16_t midiToPeriod(int midiNote, int channel) const;

  // Clock the APU and mix output
  void clockAPU(int cpuClocks);

  // NSFPlay cores
  std::unique_ptr<xgm::NES_APU> m_apu1; // Pulse channels
  std::unique_ptr<xgm::NES_DMC> m_apu2; // Triangle, Noise, DMC

  // Blip_Buffer for bandlimited synthesis
  std::unique_ptr<Blip_Buffer> m_blipBuffer;

  // Sample rate and timing
  double m_sampleRate = 44100.0;
  double m_clockRate = 1789772.7; // NTSC CPU clock
  double m_clocksPerSample = 0.0;
  double m_clockAccumulator = 0.0;

  // Channel state
  bool m_channelEnabled[NUM_CHANNELS] = {true, true, true, true, false};
  int m_currentNote[NUM_CHANNELS] = {-1, -1, -1, -1, -1};
  float m_velocity[NUM_CHANNELS] = {0, 0, 0, 0, 0};
  DutyCycle m_pulseDuty[2] = {DUTY_50, DUTY_50};
  bool m_noiseShortMode = false;

  // Temporary buffer for Blip_Buffer output
  static constexpr int TEMP_BUFFER_SIZE = 4096;
  int16_t m_tempBuffer[TEMP_BUFFER_SIZE];
};
