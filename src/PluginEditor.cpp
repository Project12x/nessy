#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
// NES-inspired color palette
const juce::Colour kBackgroundColor{0xff1d1d1d}; // Dark gray
const juce::Colour kPrimaryColor{0xffe74c3c};    // NES red
const juce::Colour kSecondaryColor{0xff3498db};  // NES blue
const juce::Colour kTextColor{0xfff0f0f0};       // Off-white
const juce::Colour kAccentColor{0xff27ae60};     // Green
const juce::Colour kOrangeColor{0xfff39c12};     // Orange for Noise

// Load embedded fonts
juce::Typeface::Ptr loadTypeface(const char *data, size_t size) {
  return juce::Typeface::createSystemTypefaceFor(data, size);
}

juce::Typeface::Ptr getInterRegular() {
  static auto typeface = loadTypeface(BinaryData::InterRegular_ttf,
                                      BinaryData::InterRegular_ttfSize);
  return typeface;
}

juce::Typeface::Ptr getInterBold() {
  static auto typeface =
      loadTypeface(BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
  return typeface;
}

juce::Font getBodyFont(float height) {
  return juce::Font(juce::FontOptions(getInterRegular()).withHeight(height));
}

juce::Font getTitleFont(float height) {
  return juce::Font(juce::FontOptions(getInterBold()).withHeight(height));
}
} // namespace

NessyAudioProcessorEditor::NessyAudioProcessorEditor(NessyAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState(),
               juce::MidiKeyboardComponent::horizontalKeyboard) {
  // Style the keyboard with NES colors
  keyboard.setKeyWidth(40.0f);
  keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                     juce::Colour(0xffeeeeee));
  keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                     juce::Colour(0xff333333));
  keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                     juce::Colour(0xff666666));
  keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                     kPrimaryColor.withAlpha(0.3f));
  keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                     kPrimaryColor.withAlpha(0.6f));

  addAndMakeVisible(keyboard);

  setSize(800, 500);
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(kBackgroundColor);

  // Header bar
  g.setColour(kPrimaryColor);
  g.fillRect(0, 0, getWidth(), 60);

  // Title - NESSY
  g.setColour(kTextColor);
  g.setFont(getTitleFont(32.0f));
  g.drawText("NESSY", 20, 10, 200, 40, juce::Justification::centredLeft);

  // Subtitle
  g.setFont(getBodyFont(14.0f));
  g.setColour(kTextColor.withAlpha(0.8f));
  g.drawText("NES APU Synthesizer", 20, 40, 200, 20,
             juce::Justification::centredLeft);

  // Voice mode indicator (top right)
  g.setFont(getBodyFont(12.0f));
  g.setColour(kTextColor.withAlpha(0.6f));
  g.drawText("4-Voice Poly", getWidth() - 120, 20, 100, 20,
             juce::Justification::centredRight);

  // Channel section
  auto bounds =
      getLocalBounds().reduced(20).withTrimmedTop(80).withTrimmedBottom(100);
  int channelWidth = bounds.getWidth() / 4;

  juce::StringArray channels = {"PULSE 1", "PULSE 2", "TRIANGLE", "NOISE"};
  juce::Array<juce::Colour> channelColors = {kPrimaryColor, kSecondaryColor,
                                             kAccentColor, kOrangeColor};

  for (int i = 0; i < 4; ++i) {
    auto channelBounds = bounds.removeFromLeft(channelWidth).reduced(8);

    // Channel box background
    g.setColour(channelColors[i].withAlpha(0.15f));
    g.fillRoundedRectangle(channelBounds.toFloat(), 12.0f);

    // Channel box border
    g.setColour(channelColors[i]);
    g.drawRoundedRectangle(channelBounds.toFloat(), 12.0f, 2.0f);

    // Channel name
    g.setColour(kTextColor);
    g.setFont(getTitleFont(13.0f));
    g.drawText(channels[i], channelBounds.removeFromTop(35),
               juce::Justification::centred);

    // Waveform placeholder
    auto waveformArea = channelBounds.reduced(10, 5).removeFromTop(80);
    g.setColour(channelColors[i].withAlpha(0.3f));
    g.fillRoundedRectangle(waveformArea.toFloat(), 6.0f);

    // Draw simple waveform representation
    g.setColour(channelColors[i]);
    auto centerY = waveformArea.getCentreY();
    auto width = waveformArea.getWidth();
    auto startX = waveformArea.getX();

    if (i < 2) // Pulse waves
    {
      // Draw square wave
      juce::Path path;
      float duty = (i == 0) ? 0.5f : 0.25f;
      path.startNewSubPath(startX, centerY + 15);
      path.lineTo(startX + width * duty, centerY + 15);
      path.lineTo(startX + width * duty, centerY - 15);
      path.lineTo(startX + width, centerY - 15);
      g.strokePath(path, juce::PathStrokeType(2.0f));
    } else if (i == 2) // Triangle
    {
      juce::Path path;
      path.startNewSubPath(startX, centerY);
      path.lineTo(startX + width * 0.25f, centerY - 15);
      path.lineTo(startX + width * 0.75f, centerY + 15);
      path.lineTo(startX + width, centerY);
      g.strokePath(path, juce::PathStrokeType(2.0f));
    } else // Noise
    {
      juce::Random rng(42);
      juce::Path path;
      path.startNewSubPath(startX, centerY);
      for (int x = 0; x < width; x += 4) {
        path.lineTo(startX + x, centerY + (rng.nextFloat() * 30 - 15));
      }
      g.strokePath(path, juce::PathStrokeType(1.5f));
    }
  }

  // Version info at bottom
  g.setColour(kTextColor.withAlpha(0.4f));
  g.setFont(getBodyFont(10.0f));
  g.drawText("v0.1.0 | GPL-3.0 | AntigravityLabs",
             getLocalBounds().removeFromBottom(20),
             juce::Justification::centred);
}

void NessyAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();

  // Keyboard at the bottom
  keyboard.setBounds(bounds.removeFromBottom(80));
}
