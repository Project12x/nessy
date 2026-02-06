#include "PluginProcessor.h"
#include "PluginEditor.h"
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
      juce::StringArray{"Round-Robin", "Pitch-Split"},
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

  // Add virtual keyboard events to the MIDI buffer
  keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

  // Process MIDI messages through voice allocator
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn()) {
      voiceAllocator->noteOn(message.getChannel() - 1, message.getNoteNumber(),
                             message.getFloatVelocity());
    } else if (message.isNoteOff()) {
      voiceAllocator->noteOff(message.getChannel() - 1,
                              message.getNoteNumber());
    } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
      voiceAllocator->allNotesOff();
    }
  }

  // Generate audio from APU
  apu->process(leftChannel, rightChannel, numSamples);

  // Apply master volume
  for (int i = 0; i < numSamples; ++i) {
    leftChannel[i] *= masterVolume;
    rightChannel[i] *= masterVolume;
  }
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
