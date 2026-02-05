// NessyAPU: Simplified NES 2A03 APU wrapper for VST use
// GPL-3.0 - Uses NSFPlay cores from Dn-FamiTracker

#include "NessyAPU.h"
#include "blip_buffer/Blip_Buffer.h"
#include "nsfplay/xgm/devices/Sound/nes_apu.h"
#include "nsfplay/xgm/devices/Sound/nes_dmc.h"

#include <algorithm>
#include <cmath>


// NES frequency lookup table constants
static constexpr double NES_CPU_CLOCK_NTSC = 1789772.7;
static constexpr double NES_CPU_CLOCK_PAL = 1662607.0;

// MIDI note 69 = A4 = 440Hz
static constexpr int MIDI_A4 = 69;
static constexpr double FREQ_A4 = 440.0;

NessyAPU::NessyAPU() {
  m_apu1 = std::make_unique<xgm::NES_APU>();
  m_apu2 = std::make_unique<xgm::NES_DMC>();
  m_blipBuffer = std::make_unique<Blip_Buffer>();
}

NessyAPU::~NessyAPU() = default;

void NessyAPU::initialize(double sampleRate) {
  m_sampleRate = sampleRate;
  m_clockRate = NES_CPU_CLOCK_NTSC;
  m_clocksPerSample = m_clockRate / m_sampleRate;
  m_clockAccumulator = 0.0;

  // Configure Blip_Buffer
  m_blipBuffer->clock_rate(static_cast<long>(m_clockRate));
  m_blipBuffer->set_sample_rate(static_cast<long>(m_sampleRate));

  // Configure NSFPlay cores
  m_apu1->SetClock(m_clockRate);
  m_apu1->SetRate(m_sampleRate);

  m_apu2->SetClock(m_clockRate);
  m_apu2->SetRate(m_sampleRate);
  m_apu2->SetAPU(m_apu1.get());
  m_apu2->SetPal(false); // NTSC mode

  // Disable nondeterministic behavior
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_TRI, 0);
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_NOISE, 0);

  reset();
}

void NessyAPU::reset() {
  m_apu1->Reset();
  m_apu2->Reset();
  m_blipBuffer->clear();
  m_clockAccumulator = 0.0;

  // Reset channel state
  for (int i = 0; i < NUM_CHANNELS; ++i) {
    m_currentNote[i] = -1;
    m_velocity[i] = 0.0f;
  }

  // Enable APU output (write to $4015)
  writeRegister(0x4015, 0x0F); // Enable pulse1, pulse2, triangle, noise
}

int NessyAPU::process(float *leftOutput, float *rightOutput, int numSamples) {
  // Clear output buffers
  std::fill(leftOutput, leftOutput + numSamples, 0.0f);
  std::fill(rightOutput, rightOutput + numSamples, 0.0f);

  int samplesGenerated = 0;

  while (samplesGenerated < numSamples) {
    // Calculate how many CPU clocks to run
    m_clockAccumulator += m_clocksPerSample;
    int clocksToRun = static_cast<int>(m_clockAccumulator);
    m_clockAccumulator -= clocksToRun;

    if (clocksToRun > 0) {
      clockAPU(clocksToRun);
    }

    // Get mixed output from APU
    int32_t out[2] = {0, 0};
    m_apu1->Render(out);

    int32_t out2[2] = {0, 0};
    m_apu2->Render(out2);
    out[0] += out2[0];

    // Convert to float [-1, 1]
    // NSFPlay output range is roughly 0-8191
    float sample = static_cast<float>(out[0]) / 8192.0f;
    sample = std::clamp(sample, -1.0f, 1.0f);

    leftOutput[samplesGenerated] = sample;
    rightOutput[samplesGenerated] = sample;
    ++samplesGenerated;
  }

  return samplesGenerated;
}

void NessyAPU::clockAPU(int cpuClocks) {
  // Tick frame sequencer and chips
  m_apu2->TickFrameSequence(cpuClocks);
  m_apu1->Tick(cpuClocks);
  m_apu2->Tick(cpuClocks);
}

void NessyAPU::noteOn(int channel, int midiNote, float velocity) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_currentNote[channel] = midiNote;
  m_velocity[channel] = velocity;

  uint16_t period = midiToPeriod(midiNote, channel);
  uint8_t volume = static_cast<uint8_t>(velocity * 15.0f);

  switch (channel) {
  case PULSE1: {
    // $4000: Duty, Length counter halt, Constant volume, Volume
    uint8_t duty = static_cast<uint8_t>(m_pulseDuty[0]) << 6;
    writeRegister(0x4000, duty | 0x30 | volume); // Constant volume, no decay

    // $4002-$4003: Period low and high + length counter
    writeRegister(0x4002, period & 0xFF);
    writeRegister(0x4003, ((period >> 8) & 0x07) | 0xF8); // Max length counter
    break;
  }
  case PULSE2: {
    uint8_t duty = static_cast<uint8_t>(m_pulseDuty[1]) << 6;
    writeRegister(0x4004, duty | 0x30 | volume);
    writeRegister(0x4006, period & 0xFF);
    writeRegister(0x4007, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case TRIANGLE: {
    // $4008: Linear counter (also controls length counter halt)
    writeRegister(0x4008, 0xFF); // Max linear counter

    // $400A-$400B: Period
    writeRegister(0x400A, period & 0xFF);
    writeRegister(0x400B, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case NOISE: {
    // $400C: Envelope
    writeRegister(0x400C, 0x30 | volume);

    // $400E: Mode and period
    // Period is based on note, but noise uses preset frequencies
    uint8_t noisePeriod = std::clamp(15 - (midiNote / 8), 0, 15);
    uint8_t mode = m_noiseShortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);

    // $400F: Length counter
    writeRegister(0x400F, 0xF8);
    break;
  }
  case DMC:
    // DMC is sample-based, skip for now
    break;
  }
}

void NessyAPU::noteOff(int channel) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_currentNote[channel] = -1;
  m_velocity[channel] = 0.0f;

  switch (channel) {
  case PULSE1:
    writeRegister(0x4000, 0x30); // Zero volume
    break;
  case PULSE2:
    writeRegister(0x4004, 0x30);
    break;
  case TRIANGLE:
    writeRegister(0x4008, 0x80); // Halt linear counter
    break;
  case NOISE:
    writeRegister(0x400C, 0x30);
    break;
  case DMC:
    break;
  }
}

void NessyAPU::setChannelEnabled(int channel, bool enabled) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_channelEnabled[channel] = enabled;

  // Update $4015 status register
  uint8_t status = 0;
  if (m_channelEnabled[PULSE1])
    status |= 0x01;
  if (m_channelEnabled[PULSE2])
    status |= 0x02;
  if (m_channelEnabled[TRIANGLE])
    status |= 0x04;
  if (m_channelEnabled[NOISE])
    status |= 0x08;
  if (m_channelEnabled[DMC])
    status |= 0x10;

  writeRegister(0x4015, status);
}

void NessyAPU::setPulseDuty(int pulseChannel, DutyCycle duty) {
  if (pulseChannel < 0 || pulseChannel > 1)
    return;

  m_pulseDuty[pulseChannel] = duty;

  // If note is playing, update the register
  int channel = (pulseChannel == 0) ? PULSE1 : PULSE2;
  if (m_currentNote[channel] >= 0) {
    uint8_t dutyBits = static_cast<uint8_t>(duty) << 6;
    uint8_t volume = static_cast<uint8_t>(m_velocity[channel] * 15.0f);
    uint16_t addr = (pulseChannel == 0) ? 0x4000 : 0x4004;
    writeRegister(addr, dutyBits | 0x30 | volume);
  }
}

void NessyAPU::setNoiseMode(bool shortMode) {
  m_noiseShortMode = shortMode;

  // Update if noise is playing
  if (m_currentNote[NOISE] >= 0) {
    uint8_t noisePeriod = std::clamp(15 - (m_currentNote[NOISE] / 8), 0, 15);
    uint8_t mode = shortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);
  }
}

double NessyAPU::getChannelFrequency(int channel) const {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return 0.0;

  int note = m_currentNote[channel];
  if (note < 0)
    return 0.0;

  // Calculate frequency from MIDI note
  return FREQ_A4 * std::pow(2.0, (note - MIDI_A4) / 12.0);
}

void NessyAPU::writeRegister(uint16_t address, uint8_t value) {
  m_apu1->Write(address, value);
  m_apu2->Write(address, value);
}

uint16_t NessyAPU::midiToPeriod(int midiNote, int channel) const {
  // Calculate frequency from MIDI note
  double freq = FREQ_A4 * std::pow(2.0, (midiNote - MIDI_A4) / 12.0);

  // Convert to NES period
  // For pulse/triangle: period = (CPU_CLOCK / (16 * freq)) - 1
  // For triangle, the divider is 32 instead of 16
  double divider = (channel == TRIANGLE) ? 32.0 : 16.0;
  double period = (m_clockRate / (divider * freq)) - 1.0;

  // Clamp to valid range (11-bit for pulse, 11-bit for triangle)
  return static_cast<uint16_t>(std::clamp(period, 0.0, 2047.0));
}
