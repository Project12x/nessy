#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "apu/NessyAPU.h"

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
                 createParameterLayout()),
      apu(std::make_unique<NessyAPU>()) {}

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
}

void NessyAudioProcessor::releaseResources() { apu->reset(); }

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

  // Process MIDI messages
  // For simplicity, route all notes to Pulse 1 (mono mode for now)
  for (const auto metadata : midiMessages) {
    auto message = metadata.getMessage();

    if (message.isNoteOn()) {
      int noteNumber = message.getNoteNumber();
      float velocity = message.getFloatVelocity();

      // Route to Pulse 1 for now (will implement proper voice allocation later)
      apu->noteOn(NessyAPU::PULSE1, noteNumber, velocity);
    } else if (message.isNoteOff()) {
      apu->noteOff(NessyAPU::PULSE1);
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
