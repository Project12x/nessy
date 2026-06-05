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
const juce::Colour kVRC6Purple{0xff9b59b6};

juce::Typeface::Ptr loadTypeface(const char *data, size_t size) {
  return juce::Typeface::createSystemTypefaceFor(data, size);
}

juce::Typeface::Ptr getPressStart() {
  static auto typeface = loadTypeface(BinaryData::PressStart2PRegular_ttf,
                                      BinaryData::PressStart2PRegular_ttfSize);
  return typeface;
}

juce::Font getPixelFont(float height) {
  return juce::Font(juce::FontOptions(getPressStart()).withHeight(height));
}

juce::Font getNESFont(float height) { return getPixelFont(height); }

// NES-themed colors for ghostmoon combo selectors
void styleCombo(gm::ComboSelector& combo) {
  combo.setColour(gm::ComboSelector::comboBgColourId,      kDarkGreyColor);
  combo.setColour(gm::ComboSelector::comboOutlineColourId, kHeaderColor);
  combo.setColour(gm::ComboSelector::comboTextColourId,    juce::Colour(0xffcccccc));
  combo.setColour(gm::ComboSelector::comboArrowColourId,   kNESRed);
  combo.setColour(gm::ComboSelector::labelTextColourId,    kNESTextColor);
  combo.setColour(gm::ComboSelector::focusRingColourId,    kNESRed);
}

void styleToggle(gm::GmToggleButton& toggle, juce::Colour ledColor) {
  toggle.setColour(gm::GmToggleButton::buttonBgColourId,   kDarkGreyColor);
  toggle.setColour(gm::GmToggleButton::buttonBgOnColourId, ledColor.withAlpha(0.2f));
  toggle.setColour(gm::GmToggleButton::textOffColourId,    juce::Colour(0xff888888));
  toggle.setColour(gm::GmToggleButton::textOnColourId,     juce::Colour(0xffcccccc));
  toggle.setColour(gm::GmToggleButton::ledOnColourId,      ledColor);
  toggle.setColour(gm::GmToggleButton::ledGlowColourId,    ledColor.withAlpha(0.25f));
  toggle.setColour(gm::GmToggleButton::labelTextColourId,  kNESTextColor);
  toggle.setColour(gm::GmToggleButton::focusRingColourId,  ledColor);
}

void styleScope(gm::Oscilloscope& scope, juce::Colour waveColor) {
  scope.setColour(gm::Oscilloscope::backgroundColourId, kDarkGreyColor);
  scope.setColour(gm::Oscilloscope::waveformColourId,   waveColor);
  scope.setColour(gm::Oscilloscope::gridColourId,       juce::Colour(0xff444444));
  scope.setColour(gm::Oscilloscope::borderColourId,     kHeaderColor);
}

// Channel accent colors indexed: P1, P2, TRI, NSE, VRC6_P1, VRC6_P2, VRC6_SAW
const juce::Colour kChannelColors[7] = {
    kPrimaryColor, kSecondaryColor, kAccentColor, kOrangeColor,
    kVRC6Purple, kVRC6Purple.brighter(0.2f), kVRC6Purple.darker(0.1f)};

} // namespace

NessyAudioProcessorEditor::NessyAudioProcessorEditor(NessyAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.getKeyboardState(),
               juce::MidiKeyboardComponent::horizontalKeyboard) {
  // Load background image from binary data
  backgroundImage = juce::ImageFileFormat::loadFrom(
      BinaryData::background_png, BinaryData::background_pngSize);

  auto &apvts = processorRef.getAPVTS();

  // --- Master volume knob ---
  masterVolume = std::make_unique<gm::Knob>(apvts, "masterVolume", "VOL", "Master volume");
  masterVolume->setColour(gm::Knob::labelTextColourId, kNESTextColor);
  masterVolume->setColour(gm::Knob::valueTextColourId, kNESTextColor);
  addAndMakeVisible(*masterVolume);

  // --- Channel enable toggles ---
  auto makeToggle = [&](std::unique_ptr<gm::GmToggleButton>& btn,
                        const juce::String& paramId, const juce::String& label,
                        juce::Colour color) {
    btn = std::make_unique<gm::GmToggleButton>();
    btn->setup(apvts, paramId, label);
    styleToggle(*btn, color);
    addAndMakeVisible(*btn);
  };

  makeToggle(pulse1Toggle,   "pulse1Enable",   "P1",    kPrimaryColor);
  makeToggle(pulse2Toggle,   "pulse2Enable",   "P2",    kSecondaryColor);
  makeToggle(triangleToggle, "triangleEnable", "TRI",   kAccentColor);
  makeToggle(noiseToggle,    "noiseEnable",    "NSE",   kOrangeColor);
  makeToggle(noiseModeToggle,"noiseMode",      "Short", kOrangeColor);
  makeToggle(portamentoToggle, "portamentoEnable", "Porta", kOrangeColor);
  makeToggle(vrc6EnableToggle, "vrc6Enable",   "VRC6",  kVRC6Purple);

  // --- Duty cycle selectors ---
  const juce::StringArray dutyChoices = {"12.5%", "25%", "50%", "75%"};
  auto makeCombo = [&](std::unique_ptr<gm::ComboSelector>& sel,
                       const juce::String& paramId, const juce::String& label,
                       const juce::StringArray& choices) {
    sel = std::make_unique<gm::ComboSelector>();
    sel->setup(apvts, paramId, label, choices);
    styleCombo(*sel);
    addAndMakeVisible(*sel);
  };

  makeCombo(pulse1Duty, "pulse1Duty", "Duty", dutyChoices);
  makeCombo(pulse2Duty, "pulse2Duty", "Duty", dutyChoices);

  // --- Voice mode ---
  makeCombo(voiceMode, "voiceMode", "Voice", {"Round-Robin", "Pitch-Split", "Unison"});

  // --- Split point ---
  splitPoint = std::make_unique<gm::HSlider>(apvts, "splitPoint", "Split", "Low", "High");
  splitPoint->setColour(gm::HSlider::labelTextColourId, kNESTextColor);
  addAndMakeVisible(*splitPoint);

  // --- Portamento speed ---
  portamentoSpeed = std::make_unique<gm::HSlider>(apvts, "portamentoSpeed", "Speed", "Slow", "Fast");
  portamentoSpeed->setColour(gm::HSlider::labelTextColourId, kNESTextColor);
  addAndMakeVisible(*portamentoSpeed);

  // --- VRC6 duty boxes (8 levels) ---
  const juce::StringArray vrc6DutyChoices = {
      "6.25%", "12.5%", "18.75%", "25%", "31.25%", "37.5%", "43.75%", "50%"};
  makeCombo(vrc6Pulse1Duty, "vrc6Pulse1Duty", "P1 Duty", vrc6DutyChoices);
  makeCombo(vrc6Pulse2Duty, "vrc6Pulse2Duty", "P2 Duty", vrc6DutyChoices);

  // --- Macro preset combo boxes ---
  const char* macroParamIds[] = {
      "macroPulse1", "macroPulse2", "macroTri", "macroNoise",
      "macroVrc6P1", "macroVrc6P2", "macroVrc6Saw"};
  const juce::StringArray macroItems = {
      "No Macro", "Plain", "Vibrato", "Vol Decay",
      "Arp Maj", "Arp Min", "Duty Sweep", "Stab"};
  for (int i = 0; i < 7; ++i)
    makeCombo(macroBoxes[i], macroParamIds[i], "Macro", macroItems);

  // --- Hardware Sweep (Pulse 1 & 2) ---
  const char* sweepEnableIds[] = {"sweep1Enable", "sweep2Enable"};
  const char* sweepDirIds[]    = {"sweep1Dir",    "sweep2Dir"};
  const char* sweepRateIds[]   = {"sweep1Rate",   "sweep2Rate"};
  const char* sweepShiftIds[]  = {"sweep1Shift",  "sweep2Shift"};

  juce::StringArray rateChoices, shiftChoices;
  for (int j = 0; j <= 7; ++j) {
    rateChoices.add("R: " + juce::String(j));
    shiftChoices.add("S: " + juce::String(j));
  }

  for (int i = 0; i < 2; ++i) {
    makeToggle(sweepEnables[i], sweepEnableIds[i], "Sweep",
               i == 0 ? kPrimaryColor : kSecondaryColor);
    makeCombo(sweepDirs[i],   sweepDirIds[i],   "Dir",   {"Down", "Up"});
    makeCombo(sweepRates[i],  sweepRateIds[i],  "Rate",  rateChoices);
    makeCombo(sweepShifts[i], sweepShiftIds[i], "Shift", shiftChoices);
  }

  // --- Arpeggiator controls ---
  makeToggle(arpToggle, "arpEnable", "Arp", kNESRed);
  makeCombo(arpPattern, "arpPattern", "Pattern", {"Up", "Down", "UpDown", "Random"});
  makeCombo(arpOctaves, "arpOctaves", "Octaves", {"1", "2", "3", "4"});

  // --- Oscilloscopes ---
  for (int i = 0; i < 7; ++i) {
    scopes[i] = std::make_unique<gm::Oscilloscope>();
    styleScope(*scopes[i], kChannelColors[i]);
    scopes[i]->setNumGridDivisions(2, 4);
    addAndMakeVisible(*scopes[i]);
  }

  // --- Keyboard styling ---
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

  setSize(900, 560);
  startTimerHz(60);
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::timerCallback() {
  // Feed APU visualizer buffers into ghostmoon oscilloscopes
  auto *apu = processorRef.getAPU();
  if (apu != nullptr) {
    for (int i = 0; i < 7; ++i) {
      int ch = (i < 4) ? i : (NessyAPU::VRC6_PULSE1 + (i - 4));
      const float* buf = apu->getVisualizerBuffer(ch);
      if (buf != nullptr)
        scopes[i]->process(buf, NessyAPU::VISUALIZER_BUFFER_SIZE);
    }
  }
  repaint(); // keyboard + background
}

void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  // Main background (Tiled NES pattern)
  if (backgroundImage.isValid()) {
    g.setTiledImageFill(backgroundImage, 0, 0, 1.0f);
    g.fillAll();
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

    g.setColour(baseColors[i].withAlpha(0.15f));
    g.fillRoundedRectangle(channelRect.toFloat(), 5.0f);

    g.setColour(baseColors[i].withAlpha(0.3f));
    g.drawRoundedRectangle(channelRect.toFloat(), 5.0f, 1.5f);

    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRoundedRectangle(channelRect.toFloat().translated(0, 1), 5.0f, 1.0f);

    g.setColour(kNESTextColor);
    g.setFont(getNESFont(10.0f));
    g.drawText(baseNames[i], x, channelArea.getY() - 25, baseChannelWidth - 10,
               20, juce::Justification::centred);
  }

  // VRC6 section
  int vrc6Width = (channelArea.getWidth() - volumeWidth - sectionGap) / 3;
  auto vrc6X = baseX + baseWidth + sectionGap;

  g.setColour(kVRC6Purple.withAlpha(0.4f));
  g.fillRect(vrc6X - 5, channelArea.getY(), 2, channelArea.getHeight());

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
  voiceMode->setBounds(getWidth() - 150, 10, 130, 38);
  portamentoToggle->setBounds(getWidth() - 340, 10, 70, 38);
  portamentoSpeed->setBounds(getWidth() - 260, 10, 100, 38);
  splitPoint->setBounds(getWidth() - 260, 38, 100, 28);

  // Arpeggiator controls
  arpToggle->setBounds(getWidth() - 520, 10, 60, 38);
  arpPattern->setBounds(getWidth() - 455, 10, 80, 38);
  arpOctaves->setBounds(getWidth() - 370, 10, 50, 38);

  // Layout constants (synced with paint())
  const int volumeWidth = 90;
  const int sectionGap = 20;
  auto channelArea =
      getLocalBounds().reduced(20).withTrimmedTop(80).withTrimmedBottom(90);

  // Base APU: 4 channels
  int baseWidth = (channelArea.getWidth() - volumeWidth - sectionGap) * 2 / 3;
  int baseChannelWidth = baseWidth / 4;
  auto baseX = channelArea.getX() + volumeWidth;

  // Volume knob on left
  masterVolume->setBounds(channelArea.getX(), channelArea.getY() + 20, 80, 90);

  // Base APU channel controls
  gm::GmToggleButton* toggles[] = {pulse1Toggle.get(), pulse2Toggle.get(),
                                    triangleToggle.get(), noiseToggle.get()};
  gm::ComboSelector* dutyBoxes[] = {pulse1Duty.get(), pulse2Duty.get(),
                                     nullptr, nullptr};

  for (int i = 0; i < 4; ++i) {
    auto x = baseX + i * baseChannelWidth;
    auto channelRect = juce::Rectangle<int>(
        x, channelArea.getY(), baseChannelWidth - 10, channelArea.getHeight());

    // Enable toggle
    toggles[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 30,
                          channelRect.getWidth() - 20, 38);

    // Duty cycle or noise mode
    if (i < 2 && dutyBoxes[i] != nullptr) {
      dutyBoxes[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 68,
                              channelRect.getWidth() - 20, 38);
    } else if (i == 3) {
      noiseModeToggle->setBounds(channelRect.getX() + 10,
                                 channelRect.getY() + 68,
                                 channelRect.getWidth() - 20, 38);
    }

    // Macro preset box
    macroBoxes[i]->setBounds(channelRect.getX() + 10, channelRect.getY() + 106,
                             channelRect.getWidth() - 20, 38);

    // Hardware Sweep (Pulse 1 & 2 only)
    if (i < 2) {
      int yOpt = channelRect.getY() + 144;
      sweepEnables[i]->setBounds(channelRect.getX() + 10, yOpt,
                                 channelRect.getWidth() - 20, 32);
      yOpt += 34;
      sweepDirs[i]->setBounds(channelRect.getX() + 10, yOpt,
                              channelRect.getWidth() - 20, 32);
      yOpt += 34;
      int halfWidth = (channelRect.getWidth() - 24) / 2;
      sweepRates[i]->setBounds(channelRect.getX() + 10, yOpt, halfWidth, 32);
      sweepShifts[i]->setBounds(channelRect.getX() + 10 + halfWidth + 4, yOpt,
                                halfWidth, 32);
    }

    // Oscilloscope at bottom of groove
    scopes[i]->setBounds(channelRect.getX() + 5,
                         channelArea.getBottom() - 100,
                         channelRect.getWidth() - 10, 60);
  }

  // VRC6 section: 3 channels
  int vrc6Width = (channelArea.getWidth() - volumeWidth - sectionGap) / 3;
  int vrc6ChannelWidth = vrc6Width / 3;
  auto vrc6X = baseX + baseWidth + sectionGap;

  // VRC6 enable toggle
  vrc6EnableToggle->setBounds(vrc6X + 5, channelArea.getY() + 10, 70, 32);

  // VRC6 duty boxes
  vrc6Pulse1Duty->setBounds(vrc6X + 5, channelArea.getY() + 60,
                            vrc6ChannelWidth - 10, 38);
  vrc6Pulse2Duty->setBounds(vrc6X + vrc6ChannelWidth + 5,
                            channelArea.getY() + 60, vrc6ChannelWidth - 10, 38);

  // VRC6 Oscilloscopes + macro boxes
  for (int i = 0; i < 3; ++i) {
    scopes[4 + i]->setBounds(vrc6X + i * vrc6ChannelWidth + 5,
                             channelArea.getBottom() - 100,
                             vrc6ChannelWidth - 10, 60);
    macroBoxes[4 + i]->setBounds(vrc6X + i * vrc6ChannelWidth + 5,
                                 channelArea.getY() + 98,
                                 vrc6ChannelWidth - 10, 38);
  }
}
