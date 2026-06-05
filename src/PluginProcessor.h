#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>

// ghostmoon DSP utilities
#include <ghostmoon/SafetyLimiter.h>
#include <ghostmoon/DCBlocker.h>
#include <ghostmoon/CpuMeter.h>
#include <ghostmoon/ParamSmoother.h>

class NessyAPU;
class VoiceAllocator;

class NessyAudioProcessor : public juce::AudioProcessor {
public:
  NessyAudioProcessor();
  ~NessyAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  // Access to keyboard state for UI
  juce::MidiKeyboardState &getKeyboardState() { return keyboardState; }

  // Access to APVTS for UI controls
  juce::AudioProcessorValueTreeState &getAPVTS() { return parameters; }

  // Access to APU for UI oscilloscopes
  NessyAPU *getAPU() { return apu.get(); }

  // CPU load for diagnostics
  float getCpuLoad() const { return cpuMeter.getLoadPercent(); }

private:
  // Audio parameters
  juce::AudioProcessorValueTreeState parameters;

  // NES APU emulation
  std::unique_ptr<NessyAPU> apu;

  // Voice allocator
  std::unique_ptr<VoiceAllocator> voiceAllocator;

  // Keyboard state for standalone virtual keyboard
  juce::MidiKeyboardState keyboardState;

  // Current sample rate
  double currentSampleRate = 44100.0;

  // Last MIDI velocity for arpeggiator dispatch
  float lastVelocity = 0.8f;

  // ghostmoon DSP chain
  gm::SafetyLimiter safetyLimiter;
  gm::DCBlocker dcBlockerL, dcBlockerR;
  gm::CpuMeter cpuMeter;
  gm::ParamSmoother<float> volumeSmoother{0.02f};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessor)
};
