// NessyAPU: NES APU wrapper for VST use with expansion chip support
// GPL-3.0 - Uses NSFPlay cores from Dn-FamiTracker

#include "NessyAPU.h"
#include "Arpeggiator.h"
#include "NessyMemory.h"
#include "MacroEngine.h"
#include "blip_buffer/Blip_Buffer.h"
#include "nsfplay/xgm/devices/Sound/nes_apu.h"
#include "nsfplay/xgm/devices/Sound/nes_dmc.h"
#include "nsfplay/xgm/devices/Sound/nes_vrc6.h"
#include "nsfplay/xgm/devices/Sound/nes_mmc5.h"
#include "nsfplay/xgm/devices/Sound/nes_fme7.h"

// km6502 (via nes_cpu.h) defines short calling-convention macros that leak
// into JUCE headers and cause C2059 syntax errors.  Undefine them here,
// after all NSFPlay headers and before any JUCE headers.
#ifdef External
#  undef External
#endif
#ifdef Callback
#  undef Callback
#endif
#ifdef Inline
#  undef Inline
#endif
#ifdef CCall
#  undef CCall
#endif
#ifdef FastCall
#  undef FastCall
#endif

#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

// Subclass to expose protected out buffer for visualization
class VRC6Exposed : public xgm::NES_VRC6 {
public:
  const int32_t *getOut() const { return out; }
};

// NES frequency lookup table constants
static constexpr double NES_CPU_CLOCK_NTSC = 1789772.7;

// MIDI note 69 = A4 = 440Hz
static constexpr int MIDI_A4 = 69;
static constexpr double FREQ_A4 = 440.0;

NessyAPU::NessyAPU() {
  m_cpu         = std::make_unique<xgm::NES_CPU>();
  m_apu1        = std::make_unique<xgm::NES_APU>();
  m_apu2        = std::make_unique<xgm::NES_DMC>();
  // The DMC dereferences its CPU pointer unguarded (UpdateIRQ/StealCycles, incl.
  // in Reset()). Give it an idle CPU — never Start()/Exec()'d, so this is
  // behaviour-neutral for the synth; it only prevents a null-deref crash.
  m_apu2->SetCPU(m_cpu.get());
  m_vrc6        = std::make_unique<VRC6Exposed>();
  m_mmc5        = std::make_unique<xgm::NES_MMC5>();
  m_fme7        = std::make_unique<xgm::NES_FME7>();
  m_blipBuffer  = std::make_unique<Blip_Buffer>();
  m_memory      = std::make_unique<NessyMemory>();
  m_macroEngine = std::make_unique<MacroEngine>();
  m_macroEngine->setAPU(this);
  m_arpeggiator = std::make_unique<Arpeggiator>();
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
  m_apu2->SetMemory(m_memory.get());
  m_apu2->SetPal(false); // NTSC mode

  // Configure VRC6
  m_vrc6->SetClock(m_clockRate);
  m_vrc6->SetRate(m_sampleRate);

  // Configure MMC5
  m_mmc5->SetClock(m_clockRate);
  m_mmc5->SetRate(m_sampleRate);

  // Configure FME7 (note: NES_FME7 ignores SetClock/SetRate — runs at fixed DEFAULT_CLOCK)
  m_fme7->SetClock(m_clockRate);
  m_fme7->SetRate(m_sampleRate);

  // Disable nondeterministic behavior
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_TRI, 0);
  m_apu2->SetOption(xgm::NES_DMC::OPT_RANDOMIZE_NOISE, 0);

  reset();
}

void NessyAPU::reset() {
  m_apu1->Reset();
  m_apu2->Reset();
  m_vrc6->Reset();
  m_mmc5->Reset();
  m_fme7->Reset();
  m_fme7Mixer = 0x3F;
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

    // converting to float and recording per-channel
    auto recordVisualizer = [&](int channel, int32_t output, float scale) {
      float sample = static_cast<float>(output) / scale;
      // Center the unipolar signal (0..1 -> -1..1) for better scope
      // visualization
      sample = (sample * 2.0f) - 1.0f;
      m_visualizerBuffers[channel][m_visualizerWritePos[channel]] = sample;
      m_visualizerWritePos[channel] =
          (m_visualizerWritePos[channel] + 1) % VISUALIZER_BUFFER_SIZE;
    };

    // NSFPlay cores store individual outputs in m_apu1->out, m_apu2->out, etc.
    recordVisualizer(PULSE1, m_apu1->out[0], 15.0f);
    recordVisualizer(PULSE2, m_apu1->out[1], 15.0f);
    recordVisualizer(TRIANGLE, m_apu2->out[0], 15.0f);
    recordVisualizer(NOISE, m_apu2->out[1], 15.0f);
    recordVisualizer(DMC, m_apu2->out[2], 127.0f);

    if (m_vrc6Enabled) {
      auto *vrc6 = static_cast<VRC6Exposed *>(m_vrc6.get());
      const int32_t *vrc6Out = vrc6->getOut();
      recordVisualizer(VRC6_PULSE1, vrc6Out[0], 15.0f);
      recordVisualizer(VRC6_PULSE2, vrc6Out[1], 15.0f);
      recordVisualizer(VRC6_SAW, vrc6Out[2], 42.0f); // Saw accumulator max ~42
    }

    // Accurate NES non-linear mixing (NESdev APU Mixer reference)
    // Pulse: 95.88 / (8128 / (p1 + p2) + 100)
    // TND:  159.79 / (1 / (tri/8227 + noise/12241 + dmc/22638) + 100)
    // Both normalised to 0..1, then summed and mapped to -1..1
    const int32_t p1  = m_apu1->out[0]; // 0–15
    const int32_t p2  = m_apu1->out[1]; // 0–15
    const int32_t tri = m_apu2->out[0]; // 0–15
    const int32_t nse = m_apu2->out[1]; // 0–15
    const int32_t dmc = m_apu2->out[2]; // 0–127

    float pulseOut = 0.0f;
    if (p1 + p2 > 0)
      pulseOut = 95.88f / (8128.0f / static_cast<float>(p1 + p2) + 100.0f);

    float tndDenom = static_cast<float>(tri) / 8227.0f
                   + static_cast<float>(nse) / 12241.0f
                   + static_cast<float>(dmc) / 22638.0f;
    float tndOut = 0.0f;
    if (tndDenom > 0.0f)
      tndOut = 159.79f / (1.0f / tndDenom + 100.0f);

    // pulseOut + tndOut is in [0, ~0.9], map to [-1, 1]
    float mixed = (pulseOut + tndOut) * 2.0f - 0.9f;

    if (m_vrc6Enabled) {
      // VRC6 is external — sum linearly and scale to match APU level
      int32_t bufferVRC6[2] = {0};
      m_vrc6->Render(bufferVRC6);
      mixed += static_cast<float>(bufferVRC6[0]) / 65536.0f;
    }

    if (m_mmc5Enabled) {
      int32_t bufferMMC5[2] = {0};
      m_mmc5->Render(bufferMMC5);
      // MMC5 squares share the 2A03 pulse level; start at the VRC6 scale and
      // tune by ear (see TESTLATER). 65536 is the starting divisor.
      mixed += static_cast<float>(bufferMMC5[0]) / 65536.0f;
    }

    if (m_fme7Enabled) {
      int32_t bufferFME7[2] = {0};
      m_fme7->Render(bufferFME7);
      // FME7 per-channel peak is ~2700; /8000 is a starting divisor — tune by ear.
      mixed += static_cast<float>(bufferFME7[0]) / 8000.0f;
    }

    float finalSample = juce::jlimit(-1.0f, 1.0f, mixed);
    leftOutput[samplesGenerated] = finalSample;
    rightOutput[samplesGenerated] = finalSample;
    ++samplesGenerated;
  }

  return samplesGenerated;
}

void NessyAPU::clockAPU(int cpuClocks) {
  m_apu2->TickFrameSequence(cpuClocks);
  m_apu1->Tick(cpuClocks);
  m_apu2->Tick(cpuClocks);

  if (m_vrc6Enabled)
    m_vrc6->Tick(cpuClocks);

  if (m_mmc5Enabled) {
    m_mmc5->TickFrameSequence(cpuClocks); // MMC5 has its own length/env sequencer
    m_mmc5->Tick(cpuClocks);
  }

  if (m_fme7Enabled)
    m_fme7->Tick(cpuClocks); // FME7 handles its own internal division (no TickFrameSequence)

  // Macro engine + arpeggiator: fire at ~60Hz (one NES frame)
  m_macroClockAccumulator += cpuClocks;
  if (m_macroClockAccumulator >= MACRO_CLOCKS) {
    m_macroClockAccumulator -= MACRO_CLOCKS;
    m_macroEngine->tick();

    // Arpeggiator tick — dispatch note change via callback
    if (m_arpeggiator && m_arpeggiator->isEnabled() && m_arpeggiator->hasNotes()) {
      int note = m_arpeggiator->tick();
      if (note >= 0 && m_arpCallback)
        m_arpCallback(note);
    }
  }
}

void NessyAPU::noteOn(int channel, int midiNote, float velocity) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_currentNote[channel] = midiNote;
  m_velocity[channel] = velocity;

  // Notify macro engine so it can reset sequence positions
  m_macroEngine->noteOn(channel, midiNote, velocity);

  uint16_t period = midiToPeriod(midiNote, channel);
  uint8_t volume = static_cast<uint8_t>(velocity * 15.0f);

  switch (channel) {
  case PULSE1: {
    uint8_t duty = static_cast<uint8_t>(m_pulseDuty[0]) << 6;
    writeRegister(0x4000, duty | 0x30 | volume);
    // Write hardware sweep register ($4001) EPPPNSSS
    uint8_t sweep = (m_sweepConfig[0].enable ? 0x80 : 0x00) |
                    ((m_sweepConfig[0].rate & 0x07) << 4) |
                    (m_sweepConfig[0].up ? 0x08 : 0x00) |
                    (m_sweepConfig[0].shift & 0x07);
    writeRegister(0x4001, sweep);
    writeRegister(0x4002, period & 0xFF);
    writeRegister(0x4003, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case PULSE2: {
    uint8_t duty = static_cast<uint8_t>(m_pulseDuty[1]) << 6;
    writeRegister(0x4004, duty | 0x30 | volume);
    // Write hardware sweep register ($4005) EPPPNSSS
    uint8_t sweep = (m_sweepConfig[1].enable ? 0x80 : 0x00) |
                    ((m_sweepConfig[1].rate & 0x07) << 4) |
                    (m_sweepConfig[1].up ? 0x08 : 0x00) |
                    (m_sweepConfig[1].shift & 0x07);
    writeRegister(0x4005, sweep);
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
    uint8_t noisePeriod = juce::jlimit(0, 15, 15 - (midiNote / 8));
    uint8_t mode = m_noiseShortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);
    writeRegister(0x400F, 0xF8);
    break;
  }
  case DMC: {
    int slot = NessyMemory::midiToDmcSlot(midiNote);
    const DmcSample* s = m_memory->getSample(slot);
    if (!s) break;

    // Rate ($4010), address ($4012), length ($4013)
    writeRegister(0x4010, s->rateReg & 0x0F);
    writeRegister(0x4012, s->addrReg);
    writeRegister(0x4013, s->lenReg);

    // Restart: disable DMC bit then re-enable to trigger sample
    uint8_t status = 0;
    if (m_channelEnabled[PULSE1])   status |= 0x01;
    if (m_channelEnabled[PULSE2])   status |= 0x02;
    if (m_channelEnabled[TRIANGLE]) status |= 0x04;
    if (m_channelEnabled[NOISE])    status |= 0x08;

    writeRegister(0x4015, status);        // clear DMC bit
    writeRegister(0x4015, status | 0x10); // set DMC bit — triggers sample
    break;
  }

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

  // MMC5 expansion channels
  case MMC5_PULSE1: {
    m_mmc5->Write(0x5015, 0x03); // enable both MMC5 squares
    uint8_t duty = static_cast<uint8_t>(m_mmc5PulseDuty[0]) << 6;
    m_mmc5->Write(0x5000, duty | 0x30 | volume);
    m_mmc5->Write(0x5002, period & 0xFF);
    m_mmc5->Write(0x5003, ((period >> 8) & 0x07) | 0xF8);
    break;
  }
  case MMC5_PULSE2: {
    m_mmc5->Write(0x5015, 0x03);
    uint8_t duty = static_cast<uint8_t>(m_mmc5PulseDuty[1]) << 6;
    m_mmc5->Write(0x5004, duty | 0x30 | volume);
    m_mmc5->Write(0x5006, period & 0xFF);
    m_mmc5->Write(0x5007, ((period >> 8) & 0x07) | 0xF8);
    break;
  }

  // Sunsoft 5B / FME7 expansion channels (PSG square tones)
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;                  // 0,1,2
    uint16_t p = midiToFME7Period(midiNote);
    fme7Write(idx * 2,     p & 0xFF);            // period low
    fme7Write(idx * 2 + 1, (p >> 8) & 0x0F);    // period high (12-bit)
    m_fme7Mixer &= ~(1 << idx);                  // enable this tone (clear disable bit)
    fme7Write(0x07, m_fme7Mixer);
    fme7Write(0x08 + idx, volume & 0x0F);        // amplitude 0-15, fixed (D4=0)
    break;
  }
  }
}

void NessyAPU::noteOff(int channel) {
  if (channel < 0 || channel >= NUM_CHANNELS)
    return;

  m_currentNote[channel] = -1;
  m_velocity[channel] = 0.0f;

  m_macroEngine->noteOff(channel);

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

  // MMC5 note off
  case MMC5_PULSE1:
    m_mmc5->Write(0x5015, 0x02); // disable square 0, keep square 1 enabled
    m_mmc5->Write(0x5000, 0x30);
    break;
  case MMC5_PULSE2:
    m_mmc5->Write(0x5015, 0x01); // disable square 1, keep square 0 enabled
    m_mmc5->Write(0x5004, 0x30);
    break;

  // Sunsoft 5B / FME7 note off
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    fme7Write(0x08 + idx, 0x00);   // amplitude 0
    m_fme7Mixer |= (1 << idx);     // disable this tone
    fme7Write(0x07, m_fme7Mixer);
    break;
  }
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
    uint8_t noisePeriod = juce::jlimit(0, 15, 15 - (m_currentNote[NOISE] / 8));
    uint8_t mode = shortMode ? 0x80 : 0x00;
    writeRegister(0x400E, mode | noisePeriod);
  }
}

void NessyAPU::setVRC6Enabled(bool enabled) {
  m_vrc6Enabled = enabled;
  // Auto-enable/disable individual VRC6 channels
  m_channelEnabled[VRC6_PULSE1] = enabled;
  m_channelEnabled[VRC6_PULSE2] = enabled;
  m_channelEnabled[VRC6_SAW] = enabled;

  if (!enabled) {
    // Silence all VRC6 channels
    m_vrc6->Write(0x9002, 0x00);
    m_vrc6->Write(0xA002, 0x00);
    m_vrc6->Write(0xB002, 0x00);
  }
}

void NessyAPU::setMmc5Enabled(bool enabled) {
  m_mmc5Enabled = enabled;
  m_channelEnabled[MMC5_PULSE1] = enabled;
  m_channelEnabled[MMC5_PULSE2] = enabled;
  if (!enabled) {
    m_mmc5->Write(0x5000, 0x30);
    m_mmc5->Write(0x5004, 0x30);
    m_mmc5->Write(0x5015, 0x00);
  }
}

void NessyAPU::setMmc5PulseDuty(int pulseChannel, DutyCycle duty) {
  if (pulseChannel < 0 || pulseChannel > 1) return;
  m_mmc5PulseDuty[pulseChannel] = duty;
  int channel = (pulseChannel == 0) ? MMC5_PULSE1 : MMC5_PULSE2;
  if (m_currentNote[channel] >= 0) {
    uint8_t dutyBits = static_cast<uint8_t>(duty) << 6;
    uint8_t vol = static_cast<uint8_t>(m_velocity[channel] * 15.0f);
    m_mmc5->Write(pulseChannel == 0 ? 0x5000 : 0x5004, dutyBits | 0x30 | vol);
  }
}

void NessyAPU::setSunsoft5bEnabled(bool enabled) {
  m_fme7Enabled = enabled;
  m_channelEnabled[FME7_A] = enabled;
  m_channelEnabled[FME7_B] = enabled;
  m_channelEnabled[FME7_C] = enabled;
  if (!enabled) {
    fme7Write(0x08, 0x00); fme7Write(0x09, 0x00); fme7Write(0x0A, 0x00); // amps 0
    m_fme7Mixer = 0x3F;
    fme7Write(0x07, m_fme7Mixer);
  }
}

void NessyAPU::fme7Write(uint8_t reg, uint8_t val) {
  m_fme7->Write(0xC000, reg);  // latch register address
  m_fme7->Write(0xE000, val);  // write data
}

uint16_t NessyAPU::midiToFME7Period(int midiNote) const {
  double freq = FREQ_A4 * std::pow(2.0, (midiNote - MIDI_A4) / 12.0);
  // AY PSG period, no -1. The numerator is 1789772 (the FME7's hardcoded PSG
  // DEFAULT_CLOCK), NOT NTSC 1789772.7 — they must match the chip's fixed clock,
  // so do not "correct" this to NES_CPU_CLOCK_NTSC or the 5B detunes.
  double period = 1789772.0 / (32.0 * freq);
  return static_cast<uint16_t>(juce::jlimit(1.0, 4095.0, period));
}

void NessyAPU::setVRC6PulseDuty(int pulseChannel, int duty) {
  if (pulseChannel < 0 || pulseChannel > 1)
    return;
  m_vrc6PulseDuty[pulseChannel] = juce::jlimit(0, 7, duty);
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

// ---------------------------------------------------------------------------
// MacroEngine helpers
// ---------------------------------------------------------------------------

// Rewrite the period registers for a channel (used by arpeggio macro)
void NessyAPU::writeNoteRegisters(int channel, int midiNote) {
  uint16_t period = midiToPeriod(midiNote, channel);
  switch (channel) {
  case PULSE1:
    writeRegister(0x4002, period & 0xFF);
    writeRegister(0x4003, (period >> 8) & 0x07);
    break;
  case PULSE2:
    writeRegister(0x4006, period & 0xFF);
    writeRegister(0x4007, (period >> 8) & 0x07);
    break;
  case TRIANGLE:
    writeRegister(0x400A, period & 0xFF);
    writeRegister(0x400B, (period >> 8) & 0x07);
    break;
  case VRC6_PULSE1:
    m_vrc6->Write(0x9001, period & 0xFF);
    m_vrc6->Write(0x9002, 0x80 | ((period >> 8) & 0x0F));
    break;
  case VRC6_PULSE2:
    m_vrc6->Write(0xA001, period & 0xFF);
    m_vrc6->Write(0xA002, 0x80 | ((period >> 8) & 0x0F));
    break;
  case VRC6_SAW:
    m_vrc6->Write(0xB001, period & 0xFF);
    m_vrc6->Write(0xB002, 0x80 | ((period >> 8) & 0x0F));
    break;
  case MMC5_PULSE1:
    m_mmc5->Write(0x5002, period & 0xFF);
    m_mmc5->Write(0x5003, (period >> 8) & 0x07);
    break;
  case MMC5_PULSE2:
    m_mmc5->Write(0x5006, period & 0xFF);
    m_mmc5->Write(0x5007, (period >> 8) & 0x07);
    break;
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    uint16_t p = midiToFME7Period(midiNote); // 5B uses its own period mapping
    fme7Write(idx * 2,     p & 0xFF);
    fme7Write(idx * 2 + 1, (p >> 8) & 0x0F);
    break;
  }
  default: break;
  }
}

// Add a signed period offset (fine pitch macro)
void NessyAPU::writePitchOffset(int channel, int periodOffset) {
  if (m_currentNote[channel] < 0) return;
  // Chip-aware clamp: 5B and VRC6 have 12-bit period; 2A03/MMC5 have 11-bit.
  bool isFme7 = (channel >= FME7_A);
  int maxP = (channel == VRC6_PULSE1 || channel == VRC6_PULSE2 || channel == VRC6_SAW || isFme7) ? 0xFFF : 0x7FF;
  uint16_t base = isFme7 ? midiToFME7Period(m_currentNote[channel])
                         : midiToPeriod(m_currentNote[channel], channel);
  int adjusted    = static_cast<int>(base) + periodOffset;
  adjusted        = juce::jlimit(0, maxP, adjusted);
  uint16_t period = static_cast<uint16_t>(adjusted);

  switch (channel) {
  case PULSE1:   writeRegister(0x4002, period & 0xFF); writeRegister(0x4003, (period >> 8) & 0x07); break;
  case PULSE2:   writeRegister(0x4006, period & 0xFF); writeRegister(0x4007, (period >> 8) & 0x07); break;
  case TRIANGLE: writeRegister(0x400A, period & 0xFF); writeRegister(0x400B, (period >> 8) & 0x07); break;
  case VRC6_PULSE1: m_vrc6->Write(0x9001, period & 0xFF); m_vrc6->Write(0x9002, 0x80 | ((period >> 8) & 0x0F)); break;
  case VRC6_PULSE2: m_vrc6->Write(0xA001, period & 0xFF); m_vrc6->Write(0xA002, 0x80 | ((period >> 8) & 0x0F)); break;
  case VRC6_SAW:    m_vrc6->Write(0xB001, period & 0xFF); m_vrc6->Write(0xB002, 0x80 | ((period >> 8) & 0x0F)); break;
  case MMC5_PULSE1: m_mmc5->Write(0x5002, period & 0xFF); m_mmc5->Write(0x5003, (period >> 8) & 0x07); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5006, period & 0xFF); m_mmc5->Write(0x5007, (period >> 8) & 0x07); break;
  case FME7_A: case FME7_B: case FME7_C: {
    int idx = channel - FME7_A;
    fme7Write(idx * 2, period & 0xFF); fme7Write(idx * 2 + 1, (period >> 8) & 0x0F);
    break;
  }
  default: break;
  }
}

// Return the current pulse duty register value (D7:D6 of $4000/$4004)
uint8_t NessyAPU::getPulseDutyReg(int pulseIndex) const {
  return static_cast<uint8_t>(m_pulseDuty[pulseIndex]) & 0x03;
}

// Channel-aware macro volume application — routes per chip.
// Behaviour-identical to the old MacroEngine inline switch for PULSE1/PULSE2/NOISE;
// VRC6 and MMC5/5B cases are additive (new).
void NessyAPU::applyMacroVolume(int channel, uint8_t volume) {
  if (volume > 15) volume = 15;
  switch (channel) {
  case PULSE1: writeRegister(0x4000, (getPulseDutyReg(0) << 6) | 0x30 | volume); break;
  case PULSE2: writeRegister(0x4004, (getPulseDutyReg(1) << 6) | 0x30 | volume); break;
  case NOISE:  writeRegister(0x400C, 0x30 | volume); break;
  case VRC6_PULSE1: m_vrc6->Write(0x9000, (static_cast<uint8_t>(m_vrc6PulseDuty[0]) << 4) | volume); break;
  case VRC6_PULSE2: m_vrc6->Write(0xA000, (static_cast<uint8_t>(m_vrc6PulseDuty[1]) << 4) | volume); break;
  case VRC6_SAW:    m_vrc6->Write(0xB000, static_cast<uint8_t>((volume / 15.0f) * 42.0f) & 0x3F); break;
  case MMC5_PULSE1: m_mmc5->Write(0x5000, (static_cast<uint8_t>(m_mmc5PulseDuty[0]) << 6) | 0x30 | volume); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5004, (static_cast<uint8_t>(m_mmc5PulseDuty[1]) << 6) | 0x30 | volume); break;
  case FME7_A: fme7Write(0x08, volume & 0x0F); break;
  case FME7_B: fme7Write(0x09, volume & 0x0F); break;
  case FME7_C: fme7Write(0x0A, volume & 0x0F); break;
  default: break; // Triangle/DMC: no volume register
  }
}

// Channel-aware macro duty application — routes per chip.
// Behaviour-identical to old MacroEngine inline switch for PULSE1/PULSE2.
void NessyAPU::applyMacroDuty(int channel, uint8_t duty) {
  switch (channel) {
  case PULSE1:     writeRegister(0x4000, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case PULSE2:     writeRegister(0x4004, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case MMC5_PULSE1: m_mmc5->Write(0x5000, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  case MMC5_PULSE2: m_mmc5->Write(0x5004, (static_cast<uint8_t>(duty & 3) << 6) | 0x30 | 15); break;
  default: break; // 5B/VRC6 saw/triangle/noise: no duty
  }
}

// Set macro preset for a channel
void NessyAPU::setMacroPreset(int channel, int presetId) {
  m_macroEngine->setPreset(channel,
      static_cast<MacroPreset>(presetId));
}

uint16_t NessyAPU::midiToPeriod(int midiNote, int channel) const {
  double freq = FREQ_A4 * std::pow(2.0, (midiNote - MIDI_A4) / 12.0);

  // VRC6 uses same period formula as base NES
  double divider = (channel == TRIANGLE) ? 32.0 : 16.0;
  double period = (m_clockRate / (divider * freq)) - 1.0;

  // VRC6 has 12-bit period; base NES (incl. MMC5) has 11-bit
  bool isVrc6 = (channel == VRC6_PULSE1 || channel == VRC6_PULSE2 || channel == VRC6_SAW);
  double maxPeriod = isVrc6 ? 4095.0 : 2047.0;
  return static_cast<uint16_t>(juce::jlimit(0.0, maxPeriod, period));
}

void NessyAPU::setManualSweepConfig(int pulseIndex, bool enable, bool up, int rate, int shift) {
  if (pulseIndex < 0 || pulseIndex > 1) return;
  m_sweepConfig[pulseIndex].enable = enable;
  m_sweepConfig[pulseIndex].up = up;
  m_sweepConfig[pulseIndex].rate = rate;
  m_sweepConfig[pulseIndex].shift = shift;
}

void NessyAPU::setPortamento(bool enable, float speed) {
  if (m_macroEngine) {
    m_macroEngine->setPortamento(enable, speed);
  }
}

// --- Arpeggiator control ---

void NessyAPU::setArpEnabled(bool enabled) {
  if (m_arpeggiator)
    m_arpeggiator->setEnabled(enabled);
  if (m_macroEngine)
    m_macroEngine->setArpeggiatorActive(enabled);
}

void NessyAPU::setArpPattern(int patternId) {
  if (m_arpeggiator)
    m_arpeggiator->setPattern(static_cast<Arpeggiator::Pattern>(patternId));
}

void NessyAPU::setArpOctaves(int octaves) {
  if (m_arpeggiator)
    m_arpeggiator->setOctaves(octaves);
}

Arpeggiator* NessyAPU::getArpeggiator() {
  return m_arpeggiator.get();
}
