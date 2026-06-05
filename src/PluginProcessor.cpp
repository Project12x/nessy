#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "apu/Arpeggiator.h"
#include "apu/NessyAPU.h"
#include "apu/VoiceAllocator.h"

static juce::AudioProcessorValueTreeState::ParameterLayout
createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  // Master volume
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("masterVolume", 1), "Master Volume",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

  // Channel enables
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("pulse1Enable", 1), "Pulse 1 Enable", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("pulse2Enable", 1), "Pulse 2 Enable", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("triangleEnable", 1), "Triangle Enable", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("noiseEnable", 1), "Noise Enable", true));

  // Pulse 1 duty cycle (0-3: 12.5%, 25%, 50%, 75%)
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("pulse1Duty", 1), "Pulse 1 Duty",
      juce::StringArray{"12.5%", "25%", "50%", "75%"},
      2)); // Default to 50%

  // Pulse 2 duty cycle
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("pulse2Duty", 1), "Pulse 2 Duty",
      juce::StringArray{"12.5%", "25%", "50%", "75%"}, 2));

  // Noise mode (0=long, 1=short)
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("noiseMode", 1), "Noise Mode (Short)", false));

  // Voice allocation mode
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("voiceMode", 1), "Voice Mode",
      juce::StringArray{"Round-Robin", "Pitch-Split", "Unison"},
      0)); // Default to Round-Robin

  // Pitch split point (MIDI note 36-84, default 60 = C4)
  layout.add(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID("splitPoint", 1), "Split Point", 36, 84, 60));

  // VRC6 Expansion
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("vrc6Enable", 1), "VRC6 Enable", false));

  // VRC6 Pulse 1 duty (0-7: 8 levels)
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("vrc6Pulse1Duty", 1), "VRC6 Pulse 1 Duty",
      juce::StringArray{"6.25%", "12.5%", "18.75%", "25%", "31.25%", "37.5%",
                        "43.75%", "50%"},
      7)); // Default to 50%

  // VRC6 Pulse 2 duty
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("vrc6Pulse2Duty", 1), "VRC6 Pulse 2 Duty",
      juce::StringArray{"6.25%", "12.5%", "18.75%", "25%", "31.25%", "37.5%",
                        "43.75%", "50%"},
      7));

  // Macro presets per channel
  juce::StringArray macroPresetNames{
      "None", "Plain", "Vibrato", "Vol Decay",
      "Arp Maj", "Arp Min", "Duty Sweep", "Stab"};
  const char* macroIds[] = {
      "macroPulse1", "macroPulse2", "macroTri", "macroNoise",
      "macroVrc6P1", "macroVrc6P2", "macroVrc6Saw"};
  const char* macroNames[] = {
      "Pulse 1 Macro", "Pulse 2 Macro", "Triangle Macro", "Noise Macro",
      "VRC6 P1 Macro", "VRC6 P2 Macro", "VRC6 Saw Macro"};
  for (int i = 0; i < 7; ++i)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(macroIds[i], 1), macroNames[i], macroPresetNames, 0));

  // Hardware Sweep (Pulse 1 & 2 only)
  juce::StringArray sweepDirChoices{"Down", "Up"};
  juce::StringArray sweepRateChoices{"R: 0", "R: 1", "R: 2", "R: 3", "R: 4", "R: 5", "R: 6", "R: 7"};
  juce::StringArray sweepShiftChoices{"S: 0", "S: 1", "S: 2", "S: 3", "S: 4", "S: 5", "S: 6", "S: 7"};
  
  // Pulse 1 Sweep
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("sweep1Enable", 1), "Pulse 1 Sweep Enable", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep1Dir", 1), "Pulse 1 Sweep Direction", sweepDirChoices, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep1Rate", 1), "Pulse 1 Sweep Rate", sweepRateChoices, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep1Shift", 1), "Pulse 1 Sweep Shift", sweepShiftChoices, 0));
      
  // Pulse 2 Sweep
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("sweep2Enable", 1), "Pulse 2 Sweep Enable", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep2Dir", 1), "Pulse 2 Sweep Direction", sweepDirChoices, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep2Rate", 1), "Pulse 2 Sweep Rate", sweepRateChoices, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("sweep2Shift", 1), "Pulse 2 Sweep Shift", sweepShiftChoices, 0));

  // Portamento
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("portamentoEnable", 1), "Portamento Enable", false));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID("portamentoSpeed", 1), "Portamento Speed",
      juce::NormalisableRange<float>(1.0f, 255.0f, 1.0f), 16.0f));

  // Arpeggiator
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID("arpEnable", 1), "Arpeggiator Enable", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID("arpPattern", 1), "Arp Pattern",
      juce::StringArray{"Up", "Down", "UpDown", "Random"}, 0));
  layout.add(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID("arpOctaves", 1), "Arp Octaves", 1, 4, 1));

  return layout;
}

NessyAudioProcessor::NessyAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("NessyParameters"),
                 createParameterLayout()),
      apu(std::make_unique<NessyAPU>()),
      voiceAllocator(std::make_unique<VoiceAllocator>()) {
  voiceAllocator->setAPU(apu.get());

  // Arp callback: when arpeggiator ticks, route note through voice allocator
  apu->setArpCallback([this](int note) {
    voiceAllocator->arpNoteOn(note, lastVelocity);
  });
}

NessyAudioProcessor::~NessyAudioProcessor() {}

const juce::String NessyAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool NessyAudioProcessor::acceptsMidi() const { return true; }
bool NessyAudioProcessor::producesMidi() const { return false; }
bool NessyAudioProcessor::isMidiEffect() const { return false; }
double NessyAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NessyAudioProcessor::getNumPrograms() { return 1; }
int NessyAudioProcessor::getCurrentProgram() { return 0; }
void NessyAudioProcessor::setCurrentProgram(int) {}
const juce::String NessyAudioProcessor::getProgramName(int) { return {}; }
void NessyAudioProcessor::changeProgramName(int, const juce::String &) {}

void NessyAudioProcessor::prepareToPlay(double sampleRate,
                                        int /*samplesPerBlock*/) {
  currentSampleRate = sampleRate;

  // Initialize APU with host sample rate
  apu->initialize(sampleRate);

  // Apply initial channel settings from parameters
  apu->setChannelEnabled(
      NessyAPU::PULSE1,
      parameters.getRawParameterValue("pulse1Enable")->load() > 0.5f);
  apu->setChannelEnabled(
      NessyAPU::PULSE2,
      parameters.getRawParameterValue("pulse2Enable")->load() > 0.5f);
  apu->setChannelEnabled(
      NessyAPU::TRIANGLE,
      parameters.getRawParameterValue("triangleEnable")->load() > 0.5f);
  apu->setChannelEnabled(
      NessyAPU::NOISE,
      parameters.getRawParameterValue("noiseEnable")->load() > 0.5f);

  // VRC6 expansion
  apu->setVRC6Enabled(parameters.getRawParameterValue("vrc6Enable")->load() >
                      0.5f);

  // ghostmoon DSP chain
  safetyLimiter.prepare(sampleRate);
  dcBlockerL.prepare(sampleRate);
  dcBlockerR.prepare(sampleRate);
  volumeSmoother.prepare(sampleRate);
  volumeSmoother.snapTo(parameters.getRawParameterValue("masterVolume")->load());
}

void NessyAudioProcessor::releaseResources() {
  voiceAllocator->allNotesOff();
  apu->reset();
}

bool NessyAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  return true;
}

void NessyAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                       juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  auto numSamples = buffer.getNumSamples();
  auto *leftChannel = buffer.getWritePointer(0);
  auto *rightChannel = buffer.getWritePointer(1);

  // Get master volume
  float masterVolume = parameters.getRawParameterValue("masterVolume")->load();

  // Update duty cycles from parameters
  int pulse1Duty =
      static_cast<int>(parameters.getRawParameterValue("pulse1Duty")->load());
  int pulse2Duty =
      static_cast<int>(parameters.getRawParameterValue("pulse2Duty")->load());
  apu->setPulseDuty(0, static_cast<NessyAPU::DutyCycle>(pulse1Duty));
  apu->setPulseDuty(1, static_cast<NessyAPU::DutyCycle>(pulse2Duty));

  // Update noise mode
  bool noiseMode = parameters.getRawParameterValue("noiseMode")->load() > 0.5f;
  apu->setNoiseMode(noiseMode);

  // Update voice allocation mode
  int voiceMode =
      static_cast<int>(parameters.getRawParameterValue("voiceMode")->load());
  voiceAllocator->setMode(static_cast<VoiceAllocator::Mode>(voiceMode));

  // Update pitch split point
  int splitPoint =
      static_cast<int>(parameters.getRawParameterValue("splitPoint")->load());
  voiceAllocator->setSplitPoint(splitPoint);

  // Sync VRC6 enable state to voice allocator
  bool vrc6Enabled =
      parameters.getRawParameterValue("vrc6Enable")->load() > 0.5f;
  voiceAllocator->setVRC6Enabled(vrc6Enabled);
  apu->setVRC6Enabled(vrc6Enabled);

  // Sync macro presets per channel
  const char* macroIds[] = {
      "macroPulse1", "macroPulse2", "macroTri", "macroNoise",
      "macroVrc6P1", "macroVrc6P2", "macroVrc6Saw"};
  const int macroChannels[] = {
      NessyAPU::PULSE1, NessyAPU::PULSE2, NessyAPU::TRIANGLE, NessyAPU::NOISE,
      NessyAPU::VRC6_PULSE1, NessyAPU::VRC6_PULSE2, NessyAPU::VRC6_SAW};
  for (int i = 0; i < 7; ++i)
    apu->setMacroPreset(macroChannels[i],
        static_cast<int>(parameters.getRawParameterValue(macroIds[i])->load()));

  // Sync Hardware Sweep Configs
  apu->setManualSweepConfig(
      0, // Pulse 1
      parameters.getRawParameterValue("sweep1Enable")->load() > 0.5f,
      parameters.getRawParameterValue("sweep1Dir")->load() > 0.5f,
      static_cast<int>(parameters.getRawParameterValue("sweep1Rate")->load()),
      static_cast<int>(parameters.getRawParameterValue("sweep1Shift")->load()));
  apu->setManualSweepConfig(
      1, // Pulse 2
      parameters.getRawParameterValue("sweep2Enable")->load() > 0.5f,
      parameters.getRawParameterValue("sweep2Dir")->load() > 0.5f,
      static_cast<int>(parameters.getRawParameterValue("sweep2Rate")->load()),
      static_cast<int>(parameters.getRawParameterValue("sweep2Shift")->load()));

  // Sync Portamento Config
  bool portamentoEnable = parameters.getRawParameterValue("portamentoEnable")->load() > 0.5f;
  float portamentoSpeed = parameters.getRawParameterValue("portamentoSpeed")->load();
  apu->setPortamento(portamentoEnable, portamentoSpeed);

  // Sync Arpeggiator Config
  bool arpEnabled = parameters.getRawParameterValue("arpEnable")->load() > 0.5f;
  apu->setArpEnabled(arpEnabled);
  apu->setArpPattern(static_cast<int>(parameters.getRawParameterValue("arpPattern")->load()));
  apu->setArpOctaves(static_cast<int>(parameters.getRawParameterValue("arpOctaves")->load()));

  // Add virtual keyboard events to the MIDI buffer
  keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

  // Process MIDI messages
  auto* arpeggiator = apu->getArpeggiator();
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn()) {
      lastVelocity = message.getFloatVelocity();
      if (arpEnabled) {
        // Feed held notes to arpeggiator; it dispatches via 60Hz callback
        arpeggiator->noteOn(message.getNoteNumber());
      } else {
        voiceAllocator->noteOn(message.getChannel() - 1, message.getNoteNumber(),
                               message.getFloatVelocity());
      }
    } else if (message.isNoteOff()) {
      if (arpEnabled) {
        arpeggiator->noteOff(message.getNoteNumber());
        // Release when all notes released
        if (!arpeggiator->hasNotes())
          voiceAllocator->arpNoteOff();
      } else {
        voiceAllocator->noteOff(message.getChannel() - 1,
                                message.getNoteNumber());
      }
    } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
      arpeggiator->reset();
      voiceAllocator->allNotesOff();
    }
  }

  // CPU profiling start
  cpuMeter.startBlock();

  // Generate audio from APU
  apu->process(leftChannel, rightChannel, numSamples);

  // DC blocking (NES APU has DC offset, especially pulse/noise channels)
  dcBlockerL.processBlock(leftChannel, numSamples);
  dcBlockerR.processBlock(rightChannel, numSamples);

  // Smoothed master volume (prevents zipper artifacts during automation)
  for (int i = 0; i < numSamples; ++i) {
    float vol = volumeSmoother.process(masterVolume);
    leftChannel[i] *= vol;
    rightChannel[i] *= vol;
  }

  // Safety limiter (NaN/Inf guard, soft limit, hard clip)
  safetyLimiter.process(leftChannel, rightChannel, numSamples);

  // CPU profiling end
  cpuMeter.endBlock(currentSampleRate, numSamples);
}

juce::AudioProcessorEditor *NessyAudioProcessor::createEditor() {
  return new NessyAudioProcessorEditor(*this);
}

bool NessyAudioProcessor::hasEditor() const { return true; }

void NessyAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = parameters.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void NessyAudioProcessor::setStateInformation(const void *data,
                                              int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
    parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new NessyAudioProcessor();
}
