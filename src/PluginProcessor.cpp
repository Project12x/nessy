#include "PluginProcessor.h"
#include "PluginEditor.h"

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

  return layout;
}

NessyAudioProcessor::NessyAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("NessyParameters"),
                 createParameterLayout()) {}

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

  // TODO: Initialize APU emulation with sample rate
}

void NessyAudioProcessor::releaseResources() {}

bool NessyAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  return true;
}

void NessyAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                       juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  auto totalNumOutputChannels = getTotalNumOutputChannels();

  // Clear output buffer
  for (auto i = 0; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // Process MIDI messages
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn()) {
      // TODO: Trigger note on APU
      int noteNumber = message.getNoteNumber();
      float velocity = message.getFloatVelocity();
      (void)noteNumber;
      (void)velocity;
    } else if (message.isNoteOff()) {
      // TODO: Trigger note off on APU
    }
  }

  // TODO: Generate samples from APU emulation
  // For now, silence
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
