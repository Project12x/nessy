// NessyAPU: NES APU wrapper for VST use with expansion chip support
// GPL-3.0 - Uses NSFPlay cores from Dn-FamiTracker

#include "NessyAPU.h"
#include "blip_buffer/Blip_Buffer.h"
#include "nsfplay/xgm/devices/Sound/nes_apu.h"
#include "nsfplay/xgm/devices/Sound/nes_dmc.h"
#include "nsfplay/xgm/devices/Sound/nes_vrc6.h"

#include <algorithm>
#include <cmath>

// NES frequency lookup table constants
static constexpr double NES_CPU_CLOCK_NTSC = 1789772.7;

// MIDI note 69 = A4 = 440Hz
static constexpr int MIDI_A4 = 69;
static constexpr double FREQ_A4 = 440.0;

NessyAPU::NessyAPU() {
  m_apu1 = std::make_unique<xgm::NES_APU>();
  m_apu2 = std::make_unique<xgm::NES_DMC>();
  m_vrc6 = std::make_unique<xgm::NES_VRC6>();
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

  // Configure VRC6
  m_vrc6->SetClock(m_clockRate);
  m_vrc6->SetRate(m_sampleRate);

  // Disable nondeterministic behavior
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_TRI, 0);
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_NOISE, 0);

  reset();
}

void NessyAPU::reset() {
  m_apu1->Reset();
  m_apu2->Reset();
  m_vrc6->Reset();
  m_blipBuffer->clear();
  m_clockAccumulator = 0.0;

  // Reset channel state
  for (int i = 0; i < NUM_CHANNELS; ++i) {
    m_currentNote[i] = -1;
    m_velocity[i] = 0.0f;
  }

  // Enable base APU output (write to $4015)
  writeRegister(0x4015, 0x0F); // Enable pulse1, pulse2, triangle, noise
}

int NessyAPU::process(float *leftOutput, float *rightOutput, int numSamples) {
  std::fill(leftOutput, leftOutput + numSamples, 0.0f);
  std::fill(rightOutput, rightOutput + numSamples, 0.0f);

  int samplesGenerated = 0;

  while (samplesGenerated < numSamples) {
    m_clockAccumulator += m_clocksPerSample;
    int clocksToRun = static_cast<int>(m_clockAccumulator);
    m_clockAccumulator -= clocksToRun;

    if (clocksToRun > 0) {
      clockAPU(clocksToRun);
    }

    // Get mixed output from base APU
    int32_t out[2] = {0, 0};
    m_apu1->Render(out);

    int32_t out2[2] = {0, 0};
    m_apu2->Render(out2);
    out[0] += out2[0];

    // Add VRC6 output if enabled
    if (m_vrc6Enabled) {
      int32_t vrc6Out[2] = {0, 0};
      m_vrc6->Render(vrc6Out);
      out[0] += vrc6Out[0];
    }

    // Convert to float [-1, 1]
    float sample = static_cast<float>(out[0]) / 8192.0f;
    sample = std::clamp(sample, -1.0f, 1.0f);

    leftOutput[samplesGenerated] = sample;
    rightOutput[samplesGenerated] = sample;
    ++samplesGenerated;
  }

  return samplesGenerated;
}

void NessyAPU::clockAPU(int cpuClocks) {
  m_apu2->TickFrameSequence(cpuClocks);
  m_apu1->Tick(cpuClocks);
  m_apu2->Tick(cpuClocks);

  // Clock VRC6 if enabled
  if (m_vrc6Enabled) {
    m_vrc6->Tick(cpuClocks);
  }
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
    uint8_t duty = static_cast<uint8_t>(m_pulseDuty[0]) << 6;
    writeRegister(0x4000, duty | 0x30 | volume);
    writeRegister(0x4002, period & 0xFF);
    writeRegister(0x4003, ((period >> 8) & 0x07) | 0xF8);
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
    writeRegister(0x4008, 0xFF);
    writeRegister(0x400A, period & 0xFF);
    writeRegister(0x400B, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case NOISE: {
    writeRegister(0x400C, 0x30 | volume);
    uint8_t noisePeriod = std::clamp(15 - (midiNote / 8), 0, 15);
    uint8_t mode = m_noiseShortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);
    writeRegister(0x400F, 0xF8);
    break;
  }
  case DMC:
    break;

  // VRC6 expansion channels
  case VRC6_PULSE1: {
    // VRC6 Pulse 1: $9000-$9002
    // $9000: D6-D4 = Duty, D3-D0 = Volume (or D7=1 for constant volume)
    uint8_t vrc6Volume = static_cast<uint8_t>(velocity * 15.0f);
    uint8_t vrc6Duty = static_cast<uint8_t>(m_vrc6PulseDuty[0]) << 4;
    m_vrc6->Write(0x9000, vrc6Duty | vrc6Volume);
    m_vrc6->Write(0x9001, period & 0xFF);
    m_vrc6->Write(0x9002,
                  0x80 | ((period >> 8) & 0x0F)); // Enable + period high
    break;
  }
  case VRC6_PULSE2: {
    // VRC6 Pulse 2: $A000-$A002
    uint8_t vrc6Volume = static_cast<uint8_t>(velocity * 15.0f);
    uint8_t vrc6Duty = static_cast<uint8_t>(m_vrc6PulseDuty[1]) << 4;
    m_vrc6->Write(0xA000, vrc6Duty | vrc6Volume);
    m_vrc6->Write(0xA001, period & 0xFF);
    m_vrc6->Write(0xA002, 0x80 | ((period >> 8) & 0x0F));
    break;
  }
  case VRC6_SAW: {
    // VRC6 Sawtooth: $B000-$B002
    // $B000: D5-D0 = Accumulator rate (volume)
    uint8_t sawVolume = static_cast<uint8_t>(velocity * 42.0f); // 0-42 range
    m_vrc6->Write(0xB000, sawVolume & 0x3F);
    m_vrc6->Write(0xB001, period & 0xFF);
    m_vrc6->Write(0xB002, 0x80 | ((period >> 8) & 0x0F));
    break;
  }
  }
}

void NessyAPU::noteOff(int channel) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_currentNote[channel] = -1;
  m_velocity[channel] = 0.0f;

  switch (channel) {
  case PULSE1:
    writeRegister(0x4000, 0x30);
    break;
  case PULSE2:
    writeRegister(0x4004, 0x30);
    break;
  case TRIANGLE:
    writeRegister(0x4008, 0x80);
    break;
  case NOISE:
    writeRegister(0x400C, 0x30);
    break;
  case DMC:
    break;

  // VRC6 note off
  case VRC6_PULSE1:
    m_vrc6->Write(0x9002, 0x00); // Disable channel
    break;
  case VRC6_PULSE2:
    m_vrc6->Write(0xA002, 0x00);
    break;
  case VRC6_SAW:
    m_vrc6->Write(0xB002, 0x00);
    break;
  }
}

void NessyAPU::setChannelEnabled(int channel, bool enabled) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_channelEnabled[channel] = enabled;

  // Update $4015 for base APU
  if (channel <= DMC) {
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
}

void NessyAPU::setPulseDuty(int pulseChannel, DutyCycle duty) {
  if (pulseChannel < 0 || pulseChannel > 1)
    return;

  m_pulseDuty[pulseChannel] = duty;

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

  if (m_currentNote[NOISE] >= 0) {
    uint8_t noisePeriod = std::clamp(15 - (m_currentNote[NOISE] / 8), 0, 15);
    uint8_t mode = shortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);
  }
}

void NessyAPU::setVRC6Enabled(bool enabled) {
  m_vrc6Enabled = enabled;
  if (!enabled) {
    // Silence all VRC6 channels
    m_vrc6->Write(0x9002, 0x00);
    m_vrc6->Write(0xA002, 0x00);
    m_vrc6->Write(0xB002, 0x00);
  }
}

void NessyAPU::setVRC6PulseDuty(int pulseChannel, int duty) {
  if (pulseChannel < 0 || pulseChannel > 1)
    return;
  m_vrc6PulseDuty[pulseChannel] = std::clamp(duty, 0, 7);
}

double NessyAPU::getChannelFrequency(int channel) const {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return 0.0;

  int note = m_currentNote[channel];
  if (note < 0)
    return 0.0;

  return FREQ_A4 * std::pow(2.0, (note - MIDI_A4) / 12.0);
}

void NessyAPU::writeRegister(uint16_t address, uint8_t value) {
  m_apu1->Write(address, value);
  m_apu2->Write(address, value);
}

uint16_t NessyAPU::midiToPeriod(int midiNote, int channel) const {
  double freq = FREQ_A4 * std::pow(2.0, (midiNote - MIDI_A4) / 12.0);

  // VRC6 uses same period formula as base NES
  double divider = (channel == TRIANGLE) ? 32.0 : 16.0;
  double period = (m_clockRate / (divider * freq)) - 1.0;

  // VRC6 has 12-bit period, base NES has 11-bit
  double maxPeriod = (channel >= VRC6_PULSE1) ? 4095.0 : 2047.0;
  return static_cast<uint16_t>(std::clamp(period, 0.0, maxPeriod));
}
