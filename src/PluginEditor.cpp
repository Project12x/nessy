#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
// NES-inspired color palette
const juce::Colour kBackgroundColor{0xff1d1d1d};
const juce::Colour kHeaderColor{0xff2a2a2a};
const juce::Colour kPrimaryColor{0xffe74c3c};   // NES red
const juce::Colour kSecondaryColor{0xff3498db}; // NES blue
const juce::Colour kTextColor{0xfff0f0f0};
const juce::Colour kAccentColor{0xff27ae60}; // Green
const juce::Colour kOrangeColor{0xfff39c12};

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
  auto &apvts = processorRef.getAPVTS();

  // Master volume slider
  masterVolumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
  masterVolumeSlider.setColour(juce::Slider::rotarySliderFillColourId,
                               kPrimaryColor);
  masterVolumeSlider.setColour(juce::Slider::thumbColourId, kTextColor);
  addAndMakeVisible(masterVolumeSlider);
  masterVolumeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "masterVolume", masterVolumeSlider);

  // Channel toggles
  auto setupToggle = [this](juce::ToggleButton &toggle, juce::Colour color) {
    toggle.setColour(juce::ToggleButton::tickColourId, color);
    toggle.setColour(juce::ToggleButton::tickDisabledColourId,
                     color.withAlpha(0.3f));
    addAndMakeVisible(toggle);
  };

  setupToggle(pulse1Toggle, kPrimaryColor);
  setupToggle(pulse2Toggle, kSecondaryColor);
  setupToggle(triangleToggle, kAccentColor);
  setupToggle(noiseToggle, kOrangeColor);

  pulse1Attachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "pulse1Enable", pulse1Toggle);
  pulse2Attachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "pulse2Enable", pulse2Toggle);
  triangleAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "triangleEnable", triangleToggle);
  noiseAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "noiseEnable", noiseToggle);

  // Duty cycle combo boxes
  auto setupDutyBox = [this](juce::ComboBox &box) {
    box.addItem("12.5%", 1);
    box.addItem("25%", 2);
    box.addItem("50%", 3);
    box.addItem("75%", 4);
    box.setColour(juce::ComboBox::backgroundColourId, kHeaderColor);
    box.setColour(juce::ComboBox::textColourId, kTextColor);
    box.setColour(juce::ComboBox::outlineColourId,
                  kPrimaryColor.withAlpha(0.5f));
    addAndMakeVisible(box);
  };

  setupDutyBox(pulse1DutyBox);
  setupDutyBox(pulse2DutyBox);

  pulse1DutyAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "pulse1Duty", pulse1DutyBox);
  pulse2DutyAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "pulse2Duty", pulse2DutyBox);

  // Voice mode selector
  voiceModeBox.addItem("Mono", 1);
  voiceModeBox.addItem("Poly 4", 2);
  voiceModeBox.addItem("Split", 3);
  voiceModeBox.setColour(juce::ComboBox::backgroundColourId, kHeaderColor);
  voiceModeBox.setColour(juce::ComboBox::textColourId, kTextColor);
  voiceModeBox.setColour(juce::ComboBox::outlineColourId,
                         kSecondaryColor.withAlpha(0.5f));
  addAndMakeVisible(voiceModeBox);
  voiceModeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "voiceMode", voiceModeBox);

  // Noise mode toggle
  noiseModeToggle.setColour(juce::ToggleButton::tickColourId, kOrangeColor);
  addAndMakeVisible(noiseModeToggle);
  noiseModeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "noiseMode", noiseModeToggle);

  // Keyboard styling
  keyboard.setKeyWidth(35.0f);
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

  setSize(820, 520);
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(kBackgroundColor);

  // Header
  g.setColour(kPrimaryColor);
  g.fillRect(0, 0, getWidth(), 50);

  g.setColour(kTextColor);
  g.setFont(getTitleFont(28.0f));
  g.drawText("NESSY", 15, 8, 150, 34, juce::Justification::centredLeft);

  g.setFont(getBodyFont(11.0f));
  g.setColour(kTextColor.withAlpha(0.7f));
  g.drawText("NES APU Synthesizer", 15, 32, 150, 16,
             juce::Justification::centredLeft);

  // Channel section labels
  auto channelArea =
      getLocalBounds().reduced(15).withTrimmedTop(60).withTrimmedBottom(90);
  int channelWidth = (channelArea.getWidth() - 100) / 4;

  juce::StringArray channels = {"PULSE 1", "PULSE 2", "TRIANGLE", "NOISE"};
  juce::Array<juce::Colour> colors = {kPrimaryColor, kSecondaryColor,
                                      kAccentColor, kOrangeColor};

  auto channelX = channelArea.getX() + 100; // After volume knob

  for (int i = 0; i < 4; ++i) {
    auto x = channelX + i * channelWidth;
    auto channelRect = juce::Rectangle<int>(
        x, channelArea.getY(), channelWidth - 8, channelArea.getHeight());

    // Channel background
    g.setColour(colors[i].withAlpha(0.1f));
    g.fillRoundedRectangle(channelRect.toFloat(), 8.0f);

    // Channel border
    g.setColour(colors[i].withAlpha(0.5f));
    g.drawRoundedRectangle(channelRect.toFloat(), 8.0f, 1.5f);

    // Channel name
    g.setColour(kTextColor);
    g.setFont(getTitleFont(11.0f));
    g.drawText(channels[i], channelRect.removeFromTop(25),
               juce::Justification::centred);
  }

  // Volume label
  g.setColour(kTextColor);
  g.setFont(getBodyFont(10.0f));
  g.drawText("VOLUME", channelArea.getX(), channelArea.getY(), 80, 20,
             juce::Justification::centred);

  // Voice mode label
  g.drawText("VOICE MODE", getWidth() - 110, 15, 100, 14,
             juce::Justification::centred);

  // Footer
  g.setColour(kTextColor.withAlpha(0.3f));
  g.setFont(getBodyFont(9.0f));
  g.drawText("v0.1.0 | GPL-3.0 | AntigravityLabs",
             getLocalBounds().removeFromBottom(18),
             juce::Justification::centred);
}

void NessyAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();

  // Keyboard at bottom
  keyboard.setBounds(bounds.removeFromBottom(70));

  // Header area controls
  voiceModeBox.setBounds(getWidth() - 110, 30, 100, 22);

  // Channel section
  auto channelArea = bounds.reduced(15).withTrimmedTop(50);
  int channelWidth = (channelArea.getWidth() - 100) / 4;

  // Volume knob on left
  masterVolumeSlider.setBounds(channelArea.getX(), channelArea.getY() + 20, 80,
                               80);

  auto channelX = channelArea.getX() + 100;

  // Channel toggles and controls
  juce::ToggleButton *toggles[] = {&pulse1Toggle, &pulse2Toggle,
                                   &triangleToggle, &noiseToggle};
  juce::ComboBox *dutyBoxes[] = {&pulse1DutyBox, &pulse2DutyBox, nullptr,
                                 nullptr};

  for (int i = 0; i < 4; ++i) {
    auto x = channelX + i * channelWidth;
    auto channelRect = juce::Rectangle<int>(
        x, channelArea.getY(), channelWidth - 8, channelArea.getHeight());

    // Enable toggle
    toggles[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 30,
                          channelRect.getWidth() - 20, 24);

    // Duty cycle or noise mode
    if (i < 2 && dutyBoxes[i] != nullptr) {
      dutyBoxes[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 60,
                              channelRect.getWidth() - 20, 24);
    } else if (i == 3) {
      noiseModeToggle.setBounds(channelRect.getX() + 10,
                                channelRect.getY() + 60,
                                channelRect.getWidth() - 20, 24);
    }
  }
}
