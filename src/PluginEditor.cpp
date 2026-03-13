#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
// Authentic NES color palette
const juce::Colour kBackgroundColor{0xffCCCCCC}; // Light grey plastic
const juce::Colour kDarkGreyColor{0xff222222};   // Dark grey plastic / Recessed
const juce::Colour kHeaderColor{0xff333333};     // Deep grey
const juce::Colour kNESRed{0xffCC0000};          // Controller/Power red
const juce::Colour kNESTextColor{0xff222222};    // Black text on grey
const juce::Colour kNESActiveRed{0xffFF0000};    // Bright LED red

const juce::Colour kPrimaryColor{0xffCC0000};
const juce::Colour kSecondaryColor{0xff336699}; // Desaturated blue
const juce::Colour kAccentColor{0xff669933};    // Desaturated green
const juce::Colour kOrangeColor{0xffCC6600};    // Desaturated orange

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

juce::Typeface::Ptr getPressStart() {
  static auto typeface = loadTypeface(BinaryData::PressStart2PRegular_ttf,
                                      BinaryData::PressStart2PRegular_ttfSize);
  return typeface;
}

juce::Font getPixelFont(float height) {
  return juce::Font(juce::FontOptions(getPressStart()).withHeight(height));
}

// Utility for all UI labels to use the pixel font
juce::Font getNESFont(float height) { return getPixelFont(height); }
} // namespace

NessyAudioProcessorEditor::NessyAudioProcessorEditor(NessyAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState(),
               juce::MidiKeyboardComponent::horizontalKeyboard) {
  // Load background image from binary data
  backgroundImage = juce::ImageFileFormat::loadFrom(
      BinaryData::background_png, BinaryData::background_pngSize);

  auto &apvts = processorRef.getAPVTS();

  // Master volume slider
  masterVolumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  masterVolumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
  masterVolumeSlider.setColour(juce::Slider::rotarySliderFillColourId,
                               kDarkGreyColor);
  masterVolumeSlider.setColour(juce::Slider::thumbColourId, kNESRed);
  masterVolumeSlider.setColour(juce::Slider::textBoxTextColourId,
                               kNESTextColor);
  masterVolumeSlider.setColour(juce::Slider::textBoxOutlineColourId,
                               juce::Colours::transparentBlack);
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
    box.setColour(juce::ComboBox::backgroundColourId, kBackgroundColor);
    box.setColour(juce::ComboBox::textColourId, kNESTextColor);
    box.setColour(juce::ComboBox::outlineColourId, kDarkGreyColor);
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
  voiceModeBox.addItem("Round-Robin", 1);
  voiceModeBox.addItem("Pitch-Split", 2);
  voiceModeBox.addItem("Unison", 3);
  voiceModeBox.setColour(juce::ComboBox::backgroundColourId, kBackgroundColor);
  voiceModeBox.setColour(juce::ComboBox::textColourId, kNESTextColor);
  voiceModeBox.setColour(juce::ComboBox::outlineColourId, kDarkGreyColor);
  addAndMakeVisible(voiceModeBox);
  voiceModeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "voiceMode", voiceModeBox);

  // Split point slider
  splitPointSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  splitPointSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
  splitPointSlider.setColour(juce::Slider::thumbColourId, kSecondaryColor);
  splitPointSlider.setColour(juce::Slider::trackColourId, kHeaderColor);
  addAndMakeVisible(splitPointSlider);
  splitPointLabel.setColour(juce::Label::textColourId, kNESTextColor);
  splitPointLabel.setFont(getNESFont(10.0f));
  splitPointAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvts, "splitPoint", splitPointSlider);

  // Noise mode toggle
  noiseModeToggle.setColour(juce::ToggleButton::tickColourId, kOrangeColor);
  addAndMakeVisible(noiseModeToggle);
  noiseModeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "noiseMode", noiseModeToggle);

  // VRC6 Expansion controls
  vrc6EnableToggle.setColour(juce::ToggleButton::tickColourId,
                             juce::Colour(0xff9b59b6)); // Purple
  addAndMakeVisible(vrc6EnableToggle);
  vrc6EnableAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvts, "vrc6Enable", vrc6EnableToggle);

  // VRC6 duty boxes (8 levels)
  auto setupVrc6DutyBox = [this](juce::ComboBox &box) {
    box.addItem("6.25%", 1);
    box.addItem("12.5%", 2);
    box.addItem("18.75%", 3);
    box.addItem("25%", 4);
    box.addItem("31.25%", 5);
    box.addItem("37.5%", 6);
    box.addItem("43.75%", 7);
    box.addItem("50%", 8);
    box.setColour(juce::ComboBox::backgroundColourId, kBackgroundColor);
    box.setColour(juce::ComboBox::textColourId, kNESTextColor);
    box.setColour(juce::ComboBox::outlineColourId, kDarkGreyColor);
    addAndMakeVisible(box);
  };
  setupVrc6DutyBox(vrc6Pulse1DutyBox);
  setupVrc6DutyBox(vrc6Pulse2DutyBox);

  vrc6Pulse1DutyAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "vrc6Pulse1Duty", vrc6Pulse1DutyBox);
  vrc6Pulse2DutyAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvts, "vrc6Pulse2Duty", vrc6Pulse2DutyBox);

  // Macro preset combo boxes
  const char* macroParamIds[] = {
      "macroPulse1", "macroPulse2", "macroTri", "macroNoise",
      "macroVrc6P1", "macroVrc6P2", "macroVrc6Saw"};
  juce::StringArray macroItems = {
      "No Macro", "Plain", "Vibrato", "Vol Decay",
      "Arp Maj", "Arp Min", "Duty Sweep", "Stab"};
  for (int i = 0; i < 7; ++i) {
    for (int j = 0; j < macroItems.size(); ++j)
      macroBoxes[i].addItem(macroItems[j], j + 1);
    macroBoxes[i].setColour(juce::ComboBox::backgroundColourId, kBackgroundColor);
    macroBoxes[i].setColour(juce::ComboBox::textColourId, kNESTextColor);
    macroBoxes[i].setColour(juce::ComboBox::outlineColourId, kDarkGreyColor);
    addAndMakeVisible(macroBoxes[i]);
    macroAttachments[i] =
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, macroParamIds[i], macroBoxes[i]);
  }

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

  // Initialize Oscilloscopes
  auto *apu = processorRef.getAPU();
  for (int i = 0; i < 7; ++i) {
    // Mapping: P1, P2, TRI, NOISE, VRC6_P1, VRC6_P2, VRC6_SAW
    int channelIdx = i;
    if (i >= 4)
      channelIdx = NessyAPU::VRC6_PULSE1 + (i - 4);

    auto scope = std::make_unique<ChannelOscilloscope>(channelIdx, *apu);
    addAndMakeVisible(*scope);
    oscilloscopes.push_back(std::move(scope));
  }

  setSize(900, 560); // Wider for VRC6 section
  startTimerHz(60);  // High rate for smooth scopes
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  // Main background (Tiled NES pattern)
  if (backgroundImage.isValid()) {
    g.setTiledImageFill(backgroundImage, 0, 0, 1.0f);
    g.fillAll();
    // Darken slightly for readability
    g.fillAll(juce::Colours::black.withAlpha(0.2f));
  } else {
    g.fillAll(kBackgroundColor);
  }

  // Header band (Dark plastic)
  g.setColour(kDarkGreyColor);
  g.fillRect(0, 0, getWidth(), 60);

  // Power LED
  g.setColour(kNESActiveRed);
  g.fillEllipse(15, 18, 12, 12);
  // LED Glow
  {
    juce::Graphics::ScopedSaveState save(g);
    g.setOpacity(0.4f);
    g.fillEllipse(12, 15, 18, 18);
  }

  // Title
  g.setColour(juce::Colours::white);
  g.setFont(getNESFont(18.0f));
  g.drawText("NESSY", 40, 10, 200, 30, juce::Justification::centredLeft);
  g.setFont(getNESFont(8.0f));
  g.setColour(juce::Colours::white.withAlpha(0.6f));
  g.drawText("NES APU SYSTEM", 42, 36, 200, 15,
             juce::Justification::centredLeft);

  // Layout constants
  const int volumeWidth = 90;
  const int sectionGap = 20;
  auto channelArea =
      getLocalBounds().reduced(20).withTrimmedTop(80).withTrimmedBottom(90);

  // Recessed area for channels
  g.setColour(kDarkGreyColor.withAlpha(0.1f));
  g.fillRoundedRectangle(channelArea.toFloat(), 10.0f);

  // Base APU: 4 channels
  int baseWidth = (channelArea.getWidth() - volumeWidth - sectionGap) * 2 / 3;
  int baseChannelWidth = baseWidth / 4;
  auto baseX = channelArea.getX() + volumeWidth;

  juce::StringArray baseNames = {"P1", "P2", "TRI", "NSE"};
  juce::Array<juce::Colour> baseColors = {kPrimaryColor, kSecondaryColor,
                                          kAccentColor, kOrangeColor};

  for (int i = 0; i < 4; ++i) {
    auto x = baseX + i * baseChannelWidth;
    auto channelRect = juce::Rectangle<int>(
        x, channelArea.getY(), baseChannelWidth - 10, channelArea.getHeight());

    // Colorful Channel background (blended)
    g.setColour(baseColors[i].withAlpha(0.15f));
    g.fillRoundedRectangle(channelRect.toFloat(), 5.0f);

    // Recessed channel "groove" border
    g.setColour(baseColors[i].withAlpha(0.3f));
    g.drawRoundedRectangle(channelRect.toFloat(), 5.0f, 1.5f);

    // Highlights for the groove
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRoundedRectangle(channelRect.toFloat().translated(0, 1), 5.0f, 1.0f);

    // Label area
    g.setColour(kNESTextColor);
    g.setFont(getNESFont(10.0f));
    g.drawText(baseNames[i], x, channelArea.getY() - 25, baseChannelWidth - 10,
               20, juce::Justification::centred);
  }

  // VRC6 section
  int vrc6Width = (channelArea.getWidth() - volumeWidth - sectionGap) / 3;
  auto vrc6X = baseX + baseWidth + sectionGap;
  auto purple = juce::Colour(0xff9b59b6);

  // VRC6 accent band
  g.setColour(purple.withAlpha(0.4f));
  g.fillRect(vrc6X - 5, channelArea.getY(), 2, channelArea.getHeight());

  // VRC6 group header
  g.setColour(kNESTextColor);
  g.setFont(getNESFont(10.0f));
  g.drawText("VRC6", vrc6X, channelArea.getY() - 25, vrc6Width, 20,
             juce::Justification::centred);

  // CRT Scanlines overlay
  g.setColour(juce::Colours::black.withAlpha(0.08f));
  for (int y = 0; y < getHeight(); y += 3) {
    g.fillRect(0, y, getWidth(), 1);
  }

  // Footer text
  g.setColour(kNESTextColor.withAlpha(0.6f));
  g.setFont(getNESFont(8.0f));
  g.drawText("v0.2.0 | GPL-3.0 | NES HARDWARE EMULATION", 0, getHeight() - 20,
             getWidth(), 20, juce::Justification::centred);
}

void NessyAudioProcessorEditor::resized() {
  auto bounds = getLocalBounds();

  // Keyboard at bottom
  keyboard.setBounds(bounds.removeFromBottom(70));

  // Header area controls
  voiceModeBox.setBounds(getWidth() - 150, 15, 130, 22);

  // Split point slider (below voice mode)
  splitPointLabel.setBounds(getWidth() - 150, 40, 130, 15);
  splitPointSlider.setBounds(getWidth() - 260, 40, 100, 15);

  // Layout constants (Synced with paint())
  const int volumeWidth = 90;
  const int sectionGap = 20;
  auto channelArea =
      getLocalBounds().reduced(20).withTrimmedTop(80).withTrimmedBottom(90);

  // Base APU: 4 channels
  int baseWidth = (channelArea.getWidth() - volumeWidth - sectionGap) * 2 / 3;
  int baseChannelWidth = baseWidth / 4;
  auto baseX = channelArea.getX() + volumeWidth;

  // Volume knob on left
  masterVolumeSlider.setBounds(channelArea.getX(), channelArea.getY() + 30, 80,
                               80);

  // Base APU channel toggles and controls
  juce::ToggleButton *toggles[] = {&pulse1Toggle, &pulse2Toggle,
                                   &triangleToggle, &noiseToggle};
  juce::ComboBox *dutyBoxes[] = {&pulse1DutyBox, &pulse2DutyBox, nullptr,
                                 nullptr};

  for (int i = 0; i < 4; ++i) {
    auto x = baseX + i * baseChannelWidth;
    auto channelRect = juce::Rectangle<int>(
        x, channelArea.getY(), baseChannelWidth - 10, channelArea.getHeight());

    // Enable toggle
    toggles[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 40,
                          channelRect.getWidth() - 20, 24);

    // Duty cycle or noise mode
    if (i < 2 && dutyBoxes[i] != nullptr) {
      dutyBoxes[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 70,
                              channelRect.getWidth() - 20, 22);
    } else if (i == 3) {
      noiseModeToggle.setBounds(channelRect.getX() + 10,
                                channelRect.getY() + 70,
                                channelRect.getWidth() - 20, 22);
    }

    // Macro preset box (below duty/noise controls)
    macroBoxes[i].setBounds(channelRect.getX() + 10, channelRect.getY() + 98,
                            channelRect.getWidth() - 20, 22);

    // Oscilloscope at bottom of groove
    if (i < (int)oscilloscopes.size()) {
      oscilloscopes[i]->setBounds(channelRect.getX() + 5,
                                  channelArea.getBottom() - 100,
                                  channelRect.getWidth() - 10, 60);
    }
  }

  // VRC6 section: 3 channels
  int vrc6Width = (channelArea.getWidth() - volumeWidth - sectionGap) / 3;
  int vrc6ChannelWidth = vrc6Width / 3;
  auto vrc6X = baseX + baseWidth + sectionGap;

  // VRC6 enable toggle (spans section header area)
  vrc6EnableToggle.setBounds(vrc6X + 5, channelArea.getY() + 10, 70, 22);

  // VRC6 duty boxes in their respective sub-columns
  vrc6Pulse1DutyBox.setBounds(vrc6X + 5, channelArea.getY() + 70,
                              vrc6ChannelWidth - 10, 22);
  vrc6Pulse2DutyBox.setBounds(vrc6X + vrc6ChannelWidth + 5,
                              channelArea.getY() + 70, vrc6ChannelWidth - 10,
                              22);

  // VRC6 Oscilloscopes
  for (int i = 0; i < 3; ++i) {
    if (4 + i < (int)oscilloscopes.size()) {
      oscilloscopes[4 + i]->setBounds(vrc6X + i * vrc6ChannelWidth + 5,
                                      channelArea.getBottom() - 100,
                                      vrc6ChannelWidth - 10, 60);
    }
    // VRC6 macro boxes (channels 4, 5, 6)
    macroBoxes[4 + i].setBounds(vrc6X + i * vrc6ChannelWidth + 5,
                                channelArea.getY() + 98,
                                vrc6ChannelWidth - 10, 22);
  }
}
