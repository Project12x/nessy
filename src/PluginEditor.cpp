#include "PluginEditor.h"

namespace {
// NES-inspired color palette
const juce::Colour kBackgroundColor{0xff1d1d1d}; // Dark gray
const juce::Colour kPrimaryColor{0xffe74c3c};    // NES red
const juce::Colour kSecondaryColor{0xff3498db};  // NES blue
const juce::Colour kTextColor{0xfff0f0f0};       // Off-white
const juce::Colour kAccentColor{0xff27ae60};     // Green
} // namespace

NessyAudioProcessorEditor::NessyAudioProcessorEditor(NessyAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p) {
  setSize(600, 400);
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(kBackgroundColor);

  // Header
  g.setColour(kPrimaryColor);
  g.fillRect(0, 0, getWidth(), 60);

  g.setColour(kTextColor);
  g.setFont(juce::Font(28.0f).boldened());
  g.drawText("NESSY", getLocalBounds().removeFromTop(60),
             juce::Justification::centred);

  // Subtitle
  g.setFont(juce::Font(14.0f));
  g.setColour(kTextColor.withAlpha(0.6f));
  g.drawText("NES APU Synthesizer",
             getLocalBounds().removeFromTop(80).removeFromBottom(20),
             juce::Justification::centred);

  // Channel labels (placeholder for future UI)
  auto bounds = getLocalBounds().reduced(20).withTrimmedTop(80);
  int channelWidth = bounds.getWidth() / 4;

  juce::StringArray channels = {"PULSE 1", "PULSE 2", "TRIANGLE", "NOISE"};
  juce::Array<juce::Colour> channelColors = {
      kPrimaryColor, kSecondaryColor, kAccentColor, juce::Colour(0xfff39c12)};

  for (int i = 0; i < 4; ++i) {
    auto channelBounds = bounds.removeFromLeft(channelWidth).reduced(5);

    // Channel box
    g.setColour(channelColors[i].withAlpha(0.3f));
    g.fillRoundedRectangle(channelBounds.toFloat(), 8.0f);

    g.setColour(channelColors[i]);
    g.drawRoundedRectangle(channelBounds.toFloat(), 8.0f, 2.0f);

    // Channel name
    g.setColour(kTextColor);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawText(channels[i], channelBounds.removeFromTop(30),
               juce::Justification::centred);
  }

  // Version info
  g.setColour(kTextColor.withAlpha(0.4f));
  g.setFont(juce::Font(10.0f));
  g.drawText("v0.1.0 | GPL-3.0", getLocalBounds().removeFromBottom(20),
             juce::Justification::centred);
}

void NessyAudioProcessorEditor::resized() {
  // Layout will be updated when we add actual controls
}
