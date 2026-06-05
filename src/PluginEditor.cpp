#include "PluginEditor.h"
#include "BinaryData.h"

// ===========================================================================
// Front-Loader editor (Direction A). See DESIGN.md for tokens & staged plan.
// Pass 1: PCB chrome, header, channel deck w/ CRT scopes, volume dial,
// 3 persistent themes, sweep woven into P1/P2 strips, styled keyboard.
// ===========================================================================

// ---- Theme factories ------------------------------------------------------
NessyTheme NessyTheme::nes() {
  auto C = [](juce::uint32 x) { return juce::Colour(x); };
  NessyTheme t;
  t.name = "NES"; t.subtitle = "2A03 APU SYSTEM";
  t.hdrA = C(0xff46453d); t.hdrB = C(0xff302f28); t.hdrDim = C(0xffc2bdaf);
  t.stripe = C(0xffcc0000); t.wordmark = C(0xfff3efe3); t.subtitleCol = C(0xffd96a58);
  t.padShellA = C(0xffcbc7ba); t.padShellB = C(0xffb1ac9e);
  t.padPlateA = C(0xff2c2c28); t.padPlateB = C(0xff151513); t.padAccent = C(0xff8e2b22);
  t.padAbHi = C(0xffc2524a); t.padAb = C(0xff9e2420); t.padAbLo = C(0xff560d0a);
  t.railA = C(0xffcfcbbf); t.railB = C(0xffbdb8ab);
  t.chipA = C(0xffd7d3c7); t.chipB = C(0xffc3beb1);
  t.faceA = C(0xffcdc9bd); t.faceB = C(0xffbbb6a9); t.faceBorder = C(0xffa9a499);
  t.macroBg = C(0xffd9d5c9); t.nameText = C(0xff7c7868); t.railText = C(0xff5f5b52);
  t.trayA = C(0xff4c4b43); t.trayB = C(0xff34332c);
  return t;
}
NessyTheme NessyTheme::famicom() {
  auto C = [](juce::uint32 x) { return juce::Colour(x); };
  NessyTheme t;
  t.name = "FC"; t.subtitle = "FAMICOM - 2A03 APU";
  t.hdrA = C(0xff8f1b1b); t.hdrB = C(0xff5c0f0f); t.hdrDim = C(0xffe9cba2);
  t.stripe = C(0xffcaa24a); t.wordmark = C(0xfff5edda); t.subtitleCol = C(0xffe2ba50);
  t.padShellA = C(0xff8c1c1c); t.padShellB = C(0xff560d0d);
  t.padPlateA = C(0xff241010); t.padPlateB = C(0xff120606); t.padAccent = C(0xffcaa24a);
  t.padAbHi = C(0xffff6a5a); t.padAb = C(0xffd11d12); t.padAbLo = C(0xff5e0e0e);
  t.railA = C(0xffece3cf); t.railB = C(0xffdccfb4);
  t.chipA = C(0xffece3cf); t.chipB = C(0xffdccfb4);
  t.faceA = C(0xffece3cf); t.faceB = C(0xffd8ccb2); t.faceBorder = C(0xffc2b48f);
  t.macroBg = C(0xffe6dcc4); t.nameText = C(0xff9a7d52); t.railText = C(0xff7a5e34);
  t.trayA = C(0xff6c1515); t.trayB = C(0xff3a0808);
  return t;
}
NessyTheme NessyTheme::fds() {
  auto C = [](juce::uint32 x) { return juce::Colour(x); };
  NessyTheme t;
  t.name = "FDS"; t.subtitle = "DISK SYSTEM - 2A03";
  t.hdrA = C(0xff3c362a); t.hdrB = C(0xff241f15); t.hdrDim = C(0xffddcc92);
  t.stripe = C(0xffe9b81f); t.wordmark = C(0xfff2cf3a); t.subtitleCol = C(0xffe85a45);
  t.padShellA = C(0xff2c2820); t.padShellB = C(0xff161208);
  t.padPlateA = C(0xff201d16); t.padPlateB = C(0xff0e0c07); t.padAccent = C(0xffe9b81f);
  t.padAbHi = C(0xffff6a5a); t.padAb = C(0xffd11d12); t.padAbLo = C(0xff5e0e0e);
  t.railA = C(0xffe8d49a); t.railB = C(0xffd4bd72);
  t.chipA = C(0xffe8d49a); t.chipB = C(0xffd4bd72);
  t.faceA = C(0xffe9d6a0); t.faceB = C(0xffd6bf78); t.faceBorder = C(0xffb3974c);
  t.macroBg = C(0xffefe0b0); t.nameText = C(0xff7a6326); t.railText = C(0xff6b551f);
  t.trayA = C(0xff3c362a); t.trayB = C(0xff241f15);
  return t;
}
NessyTheme NessyTheme::byIndex(int i) {
  return i == 1 ? famicom() : i == 2 ? fds() : nes();
}

// ---- Strip metadata & paint primitives ------------------------------------
namespace {

const char *kShort[8] = {"P1", "P2", "TRI", "NSE", "DMC", "VP1", "VP2", "SAW"};
const char *kName[8] = {"PULSE 1", "PULSE 2", "TRIANGLE", "NOISE",
                        "SAMPLES", "VRC6 P1", "VRC6 P2", "VRC6 SAW"};
const char *kStatic[8] = {"", "", "VOL FIX", "", "6 DPCM", "", "", "6-BIT"};
const juce::Colour kColor[8] = {
    juce::Colour(0xffcc0000), juce::Colour(0xff336699), juce::Colour(0xff669933),
    juce::Colour(0xffcc6600), juce::Colour(0xffc79a3a), juce::Colour(0xff9b59b6),
    juce::Colour(0xff9b59b6), juce::Colour(0xff9b59b6)};

// macroBoxes[] index for a strip, or -1 (DMC)
int macroIdx(int s) {
  switch (s) { case 0: return 0; case 1: return 1; case 2: return 2;
    case 3: return 3; case 5: return 4; case 6: return 5; case 7: return 6;
    default: return -1; }
}

juce::Typeface::Ptr pressStart() {
  static auto tf = juce::Typeface::createSystemTypefaceFor(
      BinaryData::PressStart2PRegular_ttf,
      BinaryData::PressStart2PRegular_ttfSize);
  return tf;
}
juce::Font px(float h) {
  return juce::Font(juce::FontOptions(pressStart()).withHeight(h));
}

void vgrad(juce::Graphics &g, juce::Rectangle<float> r, juce::Colour a,
           juce::Colour b, float radius) {
  g.setGradientFill({a, r.getX(), r.getY(), b, r.getX(), r.getBottom(), false});
  if (radius > 0) g.fillRoundedRectangle(r, radius); else g.fillRect(r);
}

void bevelOut(juce::Graphics &g, juce::Rectangle<float> r, juce::Colour a,
              juce::Colour b, float radius) {
  g.setColour(juce::Colours::black.withAlpha(0.25f));
  g.fillRoundedRectangle(r.translated(0, 1.5f), radius);
  vgrad(g, r, a, b, radius);
  g.setColour(juce::Colours::white.withAlpha(0.40f));
  g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.16f));
  g.fillRect(juce::Rectangle<float>(r.getX() + radius, r.getBottom() - 2.0f,
                                    r.getWidth() - 2 * radius, 1.4f));
}

void bevelIn(juce::Graphics &g, juce::Rectangle<float> r, juce::Colour fill,
             float radius) {
  g.setColour(fill);
  g.fillRoundedRectangle(r, radius);
  g.setGradientFill({juce::Colours::black.withAlpha(0.40f), r.getX(), r.getY(),
                     juce::Colours::transparentBlack, r.getX(),
                     r.getY() + r.getHeight() * 0.55f, false});
  g.fillRoundedRectangle(r, radius);
  g.setColour(juce::Colours::white.withAlpha(0.10f));
  g.drawHorizontalLine((int)r.getBottom() - 1, r.getX() + radius,
                       r.getRight() - radius);
}

void led(juce::Graphics &g, juce::Point<float> c, float rad, juce::Colour col,
         bool on) {
  if (on) {
    g.setColour(col.withAlpha(0.28f));
    g.fillEllipse(c.x - rad * 2.2f, c.y - rad * 2.2f, rad * 4.4f, rad * 4.4f);
    g.setColour(col.withAlpha(0.45f));
    g.fillEllipse(c.x - rad * 1.4f, c.y - rad * 1.4f, rad * 2.8f, rad * 2.8f);
  }
  juce::Colour base = on ? col : juce::Colour(0xff3a3a37);
  g.setGradientFill({base.brighter(on ? 0.6f : 0.1f), c.x - rad * 0.3f,
                     c.y - rad * 0.3f, base.darker(0.5f), c.x + rad, c.y + rad,
                     true});
  g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2);
  g.setColour(juce::Colours::white.withAlpha(on ? 0.6f : 0.25f));
  g.fillEllipse(c.x - rad * 0.5f, c.y - rad * 0.6f, rad * 0.55f, rad * 0.55f);
}

void louvers(juce::Graphics &g, juce::Rectangle<int> r) {
  for (int y = r.getY(); y < r.getBottom(); y += 7) {
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillRect(r.getX(), y, r.getWidth(), 1);
    g.setColour(juce::Colours::black.withAlpha(0.09f));
    g.fillRect(r.getX(), y + 1, r.getWidth(), 1);
  }
}

void screw(juce::Graphics &g, juce::Point<float> c) {
  float rad = 4.5f;
  g.setGradientFill({juce::Colour(0xff5c5a52), c.x - rad * 0.3f, c.y - rad * 0.3f,
                     juce::Colour(0xff232220), c.x + rad, c.y + rad, true});
  g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2);
  g.setColour(juce::Colour(0xff15140f));
  juce::Path slot;
  slot.addRectangle(-rad * 0.7f, -0.7f, rad * 1.4f, 1.4f);
  g.fillPath(slot, juce::AffineTransform::rotation(0.7f).translated(c.x, c.y));
}

// CRT glass over a scope rect (called from paintOverChildren)
void crtGlass(juce::Graphics &g, juce::Rectangle<float> r) {
  auto cx = r.getCentreX(), cy = r.getCentreY();
  juce::ColourGradient vig(juce::Colours::transparentBlack, cx, cy,
                           juce::Colours::black.withAlpha(0.5f), r.getX(),
                           r.getY(), true);
  vig.addColour(0.55, juce::Colours::transparentBlack);
  g.setGradientFill(vig);
  g.fillRect(r);
  g.setGradientFill({juce::Colours::white.withAlpha(0.16f), r.getX(), r.getY(),
                     juce::Colours::transparentWhite, r.getCentreX(),
                     r.getCentreY(), false});
  g.fillRect(r);
  g.setColour(juce::Colours::black.withAlpha(0.22f));
  for (float y = r.getY(); y < r.getBottom(); y += 2.0f)
    g.fillRect(r.getX(), y, r.getWidth(), 1.0f);
}

// Row rects within a strip column (faceplate area below the colored tab).
struct Rows {
  juce::Rectangle<int> tab, onoff, readout, macro, scope, name;
  juce::Rectangle<int> swEnable, swDir, swRate, swShift;
};
Rows rowsFor(juce::Rectangle<int> s, bool hasSweep) {
  Rows r;
  const int tabH = 16, pad = 6;
  r.tab = s.removeFromTop(tabH);
  auto face = s.reduced(pad);
  auto take = [&](int h, int gap) {
    auto rr = face.removeFromTop(h);
    face.removeFromTop(gap);
    return rr;
  };
  r.onoff = take(22, 4);
  r.readout = take(22, 4);
  r.macro = take(20, 5);
  r.scope = take(50, 4);
  r.name = take(13, 4);
  if (hasSweep) {
    r.swEnable = take(20, 3);
    r.swDir = take(20, 3);
    auto rs = take(20, 0);
    r.swRate = rs.removeFromLeft(rs.getWidth() / 2 - 2);
    r.swShift = rs.removeFromRight(rs.getWidth() - 2);
  }
  return r;
}

} // namespace

// ===========================================================================
// Editor
// ===========================================================================
NessyAudioProcessorEditor::NessyAudioProcessorEditor(NessyAudioProcessor &p)
    : gm::ScaledEditor(p, kBaseW, kBaseH), processorRef(p),
      currentTheme(NessyTheme::nes()),
      keyboard(p.getKeyboardState(),
               juce::MidiKeyboardComponent::horizontalKeyboard) {
  backgroundImage = juce::ImageFileFormat::loadFrom(BinaryData::background_png,
                                                    BinaryData::background_pngSize);
  auto &apvts = processorRef.getAPVTS();

  masterVolume = std::make_unique<gm::Knob>(apvts, "masterVolume", "VOL",
                                            "Master volume");
  masterVolume->setStyle(gm::KnobStyle::Knurled);
  addAndMakeVisible(*masterVolume);

  auto mkToggle = [&](std::unique_ptr<gm::GmToggleButton> &b,
                      const juce::String &id, const juce::String &label) {
    b = std::make_unique<gm::GmToggleButton>();
    b->setup(apvts, id, label);
    addAndMakeVisible(*b);
  };
  auto mkCombo = [&](std::unique_ptr<gm::ComboSelector> &c,
                     const juce::String &id, const juce::String &label,
                     const juce::StringArray &items) {
    c = std::make_unique<gm::ComboSelector>();
    c->setup(apvts, id, label, items);
    addAndMakeVisible(*c);
  };

  mkToggle(pulse1Toggle, "pulse1Enable", "ON");
  mkToggle(pulse2Toggle, "pulse2Enable", "ON");
  mkToggle(triangleToggle, "triangleEnable", "ON");
  mkToggle(noiseToggle, "noiseEnable", "ON");
  mkToggle(vrc6EnableToggle, "vrc6Enable", "ON");
  mkToggle(noiseModeToggle, "noiseMode", "SHORT");

  const juce::StringArray duty4 = {"12.5%", "25%", "50%", "75%"};
  const juce::StringArray duty8 = {"6%",  "12%", "19%", "25%",
                                   "31%", "37%", "44%", "50%"};
  mkCombo(pulse1Duty, "pulse1Duty", "DUTY", duty4);
  mkCombo(pulse2Duty, "pulse2Duty", "DUTY", duty4);
  mkCombo(vrc6Pulse1Duty, "vrc6Pulse1Duty", "DUTY", duty8);
  mkCombo(vrc6Pulse2Duty, "vrc6Pulse2Duty", "DUTY", duty8);

  const char *macroIds[7] = {"macroPulse1", "macroPulse2", "macroTri",
                             "macroNoise", "macroVrc6P1", "macroVrc6P2",
                             "macroVrc6Saw"};
  const juce::StringArray macroItems = {"NONE",    "PLAIN",   "VIBRATO",
                                        "DECAY",   "ARP MAJ", "ARP MIN",
                                        "DUTYSWP", "STAB"};
  for (int i = 0; i < 7; ++i)
    mkCombo(macroBoxes[i], macroIds[i], "MAC", macroItems);

  // Global cluster (control rail)
  mkCombo(voiceMode, "voiceMode", "VOICE",
          {"Round-Robin", "Pitch-Split", "Unison"});
  mkToggle(arpToggle, "arpEnable", "ARP");
  mkCombo(arpPattern, "arpPattern", "PATTERN", {"Up", "Down", "UpDown", "Rand"});
  mkCombo(arpOctaves, "arpOctaves", "OCT", {"1", "2", "3", "4"});
  mkToggle(portamentoToggle, "portamentoEnable", "PORTA");
  splitPoint = std::make_unique<gm::HSlider>(apvts, "splitPoint", "SPLIT", "Lo",
                                             "Hi");
  addAndMakeVisible(*splitPoint);
  portamentoSpeed = std::make_unique<gm::HSlider>(apvts, "portamentoSpeed",
                                                  "GLIDE", "Slow", "Fast");
  addAndMakeVisible(*portamentoSpeed);

  // Hardware sweep woven into P1/P2 strips
  const char *swEn[2] = {"sweep1Enable", "sweep2Enable"};
  const char *swDir[2] = {"sweep1Dir", "sweep2Dir"};
  const char *swRate[2] = {"sweep1Rate", "sweep2Rate"};
  const char *swShift[2] = {"sweep1Shift", "sweep2Shift"};
  juce::StringArray rateItems, shiftItems;
  for (int j = 0; j <= 7; ++j) {
    rateItems.add(juce::String(j));
    shiftItems.add(juce::String(j));
  }
  for (int i = 0; i < 2; ++i) {
    mkToggle(sweepEnables[i], swEn[i], "SWEEP");
    mkCombo(sweepDirs[i], swDir[i], "DIR", {"Down", "Up"});
    mkCombo(sweepRates[i], swRate[i], "RATE", rateItems);
    mkCombo(sweepShifts[i], swShift[i], "SHF", shiftItems);
  }

  for (int i = 0; i < kNumStrips; ++i) {
    scopes[i] = std::make_unique<gm::Oscilloscope>();
    scopes[i]->setNumGridDivisions(2, 2);
    addAndMakeVisible(*scopes[i]);
  }

  keyboard.setKeyWidth(22.0f);
  addAndMakeVisible(keyboard);

  setTheme((int)apvts.state.getProperty("uiTheme", 0));
  setScale(1.0f); // triggers resizedContent()
  startTimerHz(60);
}

NessyAudioProcessorEditor::~NessyAudioProcessorEditor() {}

void NessyAudioProcessorEditor::timerCallback() {
  if (auto *apu = processorRef.getAPU()) {
    for (int i = 0; i < kNumStrips; ++i)
      if (const float *buf = apu->getVisualizerBuffer(i))
        scopes[i]->process(buf, NessyAPU::VISUALIZER_BUFFER_SIZE);
  }
  repaint();
}

// ---- Geometry -------------------------------------------------------------
juce::Rectangle<int> NessyAudioProcessorEditor::headerBounds() const {
  return {0, 0, kBaseW, 72};
}
juce::Rectangle<int> NessyAudioProcessorEditor::deckBounds() const {
  return {0, 118, kBaseW, kBaseH - 18 - 62 - 118};
}
juce::Rectangle<int> NessyAudioProcessorEditor::stripBounds(int s) const {
  auto deck = deckBounds().reduced(18, 8);
  const int gap = 8, divider = 12, n = kNumStrips;
  int sw = (deck.getWidth() - (n - 1) * gap - divider) / n;
  int x = deck.getX();
  for (int k = 0; k < s; ++k) {
    x += sw + gap;
    if (k == 4) x += divider; // divider after DMC (strip 4)
  }
  return {x, deck.getY(), sw, deck.getHeight()};
}
juce::Rectangle<int> NessyAudioProcessorEditor::themeSwitchBounds() const {
  return {kBaseW - 16 - 104, 14, 104, 44};
}
std::array<juce::Rectangle<int>, 3>
NessyAudioProcessorEditor::themeSegmentRects() const {
  auto box = themeSwitchBounds().removeFromTop(26).reduced(2);
  int w = box.getWidth() / 3;
  return {box.removeFromLeft(w), box.removeFromLeft(w), box};
}

// ---- Theme ----------------------------------------------------------------
void NessyAudioProcessorEditor::setTheme(int index) {
  themeIndex = juce::jlimit(0, 2, index);
  currentTheme = NessyTheme::byIndex(themeIndex);
  processorRef.getAPVTS().state.setProperty("uiTheme", themeIndex, nullptr);
  applyThemeToControls();
  repaint();
}

void NessyAudioProcessorEditor::applyThemeToControls() {
  const auto &t = currentTheme;
  const juce::Colour darkInset(0xff201e18);

  auto styleCombo = [&](gm::ComboSelector *c, juce::Colour bg, juce::Colour text,
                        juce::Colour arrow) {
    if (!c) return;
    c->setColour(gm::ComboSelector::comboBgColourId, bg);
    c->setColour(gm::ComboSelector::comboOutlineColourId, t.faceBorder);
    c->setColour(gm::ComboSelector::comboTextColourId, text);
    c->setColour(gm::ComboSelector::comboArrowColourId, arrow);
    c->setColour(gm::ComboSelector::labelTextColourId, t.nameText);
    c->setColour(gm::ComboSelector::focusRingColourId, t.stripe);
  };
  auto styleTog = [&](gm::GmToggleButton *b, juce::Colour ledCol) {
    if (!b) return;
    b->setColour(gm::GmToggleButton::buttonBgColourId, juce::Colour(0xff34332b));
    b->setColour(gm::GmToggleButton::buttonBgOnColourId, ledCol.withAlpha(0.22f));
    b->setColour(gm::GmToggleButton::textOffColourId, juce::Colour(0xff76736b));
    b->setColour(gm::GmToggleButton::textOnColourId, juce::Colour(0xffe6e2d6));
    b->setColour(gm::GmToggleButton::ledOnColourId, ledCol);
    b->setColour(gm::GmToggleButton::ledGlowColourId, ledCol.withAlpha(0.30f));
    b->setColour(gm::GmToggleButton::labelTextColourId, t.nameText);
    b->setColour(gm::GmToggleButton::focusRingColourId, ledCol);
  };

  // Per-strip duty combos + macro chips + scopes + enable toggles
  styleCombo(pulse1Duty.get(), darkInset, kColor[0].brighter(0.6f), t.stripe);
  styleCombo(pulse2Duty.get(), darkInset, kColor[1].brighter(0.7f), t.stripe);
  styleCombo(vrc6Pulse1Duty.get(), darkInset, kColor[5].brighter(0.7f), t.stripe);
  styleCombo(vrc6Pulse2Duty.get(), darkInset, kColor[6].brighter(0.7f), t.stripe);
  for (int i = 0; i < 7; ++i)
    styleCombo(macroBoxes[i].get(), t.macroBg, juce::Colour(0xff2a2a28),
               t.stripe.darker(0.2f));

  styleTog(pulse1Toggle.get(), kColor[0]);
  styleTog(pulse2Toggle.get(), kColor[1]);
  styleTog(triangleToggle.get(), kColor[2]);
  styleTog(noiseToggle.get(), kColor[3]);
  styleTog(noiseModeToggle.get(), kColor[3]);
  styleTog(vrc6EnableToggle.get(), kColor[5]);

  // Global cluster
  styleCombo(voiceMode.get(), darkInset, t.hdrDim, t.stripe);
  styleCombo(arpPattern.get(), darkInset, t.hdrDim, t.stripe);
  styleCombo(arpOctaves.get(), darkInset, t.hdrDim, t.stripe);
  styleTog(arpToggle.get(), t.stripe);
  styleTog(portamentoToggle.get(), kColor[3]);
  for (auto *s : {splitPoint.get(), portamentoSpeed.get()}) {
    s->setColour(gm::HSlider::labelTextColourId, t.railText);
    s->setColour(gm::HSlider::endpointTextColourId, t.railText);
  }

  // Sweep
  for (int i = 0; i < 2; ++i) {
    styleTog(sweepEnables[i].get(), kColor[i]);
    styleCombo(sweepDirs[i].get(), darkInset, t.hdrDim, t.stripe);
    styleCombo(sweepRates[i].get(), darkInset, t.hdrDim, t.stripe);
    styleCombo(sweepShifts[i].get(), darkInset, t.hdrDim, t.stripe);
  }

  // Scopes
  for (int i = 0; i < kNumStrips; ++i) {
    scopes[i]->setColour(gm::Oscilloscope::backgroundColourId,
                         juce::Colour(0xff161614));
    scopes[i]->setColour(gm::Oscilloscope::waveformColourId, kColor[i]);
    scopes[i]->setColour(gm::Oscilloscope::gridColourId, juce::Colour(0x335a5a52));
    scopes[i]->setColour(gm::Oscilloscope::borderColourId,
                         juce::Colours::transparentBlack);
  }

  // Knob + keyboard
  masterVolume->setColour(gm::Knob::labelTextColourId, t.hdrDim);
  masterVolume->setColour(gm::Knob::valueTextColourId, t.hdrDim);
  keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                     juce::Colour(0xfff4f1e9));
  keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                     juce::Colour(0xff1f1f1d));
  keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                     t.trayB.darker(0.3f));
  keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                     t.stripe.withAlpha(0.30f));
  keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                     t.stripe.withAlpha(0.55f));
}

// ---- Paint ----------------------------------------------------------------
void NessyAudioProcessorEditor::paint(juce::Graphics &g) {
  const auto &t = currentTheme;

  // PCB chassis
  if (backgroundImage.isValid()) {
    g.setTiledImageFill(backgroundImage, 0, 0, 1.0f);
    g.fillAll();
    g.fillAll(juce::Colour(0xff0a0c12).withAlpha(0.34f));
  } else {
    g.fillAll(juce::Colour(0xff14161f));
  }

  // ---- Header rail ----
  auto hdr = headerBounds();
  vgrad(g, hdr.toFloat(), t.hdrA, t.hdrB, 0);
  louvers(g, hdr);
  g.setColour(t.stripe);
  g.fillRect(0, hdr.getBottom() - 3, kBaseW, 3);
  g.setColour(juce::Colours::black.withAlpha(0.55f));
  g.fillRect(0, hdr.getBottom() - 5, kBaseW, 2);

  led(g, {26.0f, 26.0f}, 6.5f, juce::Colour(0xffcc0000), true);
  g.setColour(t.hdrDim);
  g.setFont(px(5.0f));
  g.drawText("POWER", 12, 40, 36, 10, juce::Justification::centred);
  g.setColour(t.wordmark);
  g.setFont(px(23.0f));
  g.drawText("NESSY", 50, 12, 280, 26, juce::Justification::centredLeft);
  g.setColour(t.subtitleCol);
  g.setFont(px(6.5f));
  g.drawText(t.subtitle, 52, 44, 280, 12, juce::Justification::centredLeft);

  // Theme switch
  {
    auto box = themeSwitchBounds();
    bevelIn(g, box.removeFromTop(26).toFloat(), juce::Colour(0xff0c0c0c), 5.0f);
    auto segs = themeSegmentRects();
    const char *lbl[3] = {"NES", "FC", "FDS"};
    for (int i = 0; i < 3; ++i) {
      bool on = i == themeIndex;
      if (on) {
        g.setColour(juce::Colour(0xff26261f));
        g.fillRoundedRectangle(segs[i].toFloat().reduced(1), 3.0f);
      }
      g.setColour(on ? t.stripe : t.hdrDim);
      g.setFont(px(6.5f));
      g.drawText(lbl[i], segs[i], juce::Justification::centred);
    }
    g.setColour(t.hdrDim);
    g.setFont(px(5.0f));
    g.drawText("SYSTEM", themeSwitchBounds().removeFromBottom(12),
               juce::Justification::centred);
  }

  // ---- Control rail ----
  auto rail = juce::Rectangle<int>(0, 72, kBaseW, 46).reduced(14, 6);
  bevelOut(g, rail.toFloat(), t.railA, t.railB, 6.0f);

  // ---- Channel deck ----
  auto deck = deckBounds();
  g.setColour(juce::Colour(0xff8d93a3));
  g.setFont(px(6.0f));
  g.drawText("RICOH 2A03 - BASE APU", deck.getX() + 20, deck.getY() - 2, 320, 12,
             juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xffb89cce));
  g.drawText("KONAMI VRC6", deck.getRight() - 240, deck.getY() - 2, 220, 12,
             juce::Justification::centredRight);

  screw(g, {12.0f, (float)deck.getY() + 6});
  screw(g, {(float)kBaseW - 12.0f, (float)deck.getY() + 6});
  screw(g, {12.0f, (float)deck.getBottom() - 6});
  screw(g, {(float)kBaseW - 12.0f, (float)deck.getBottom() - 6});

  bool vrc6On =
      processorRef.getAPVTS().getRawParameterValue("vrc6Enable")->load() > 0.5f;

  for (int i = 0; i < kNumStrips; ++i) {
    auto r = rowsFor(stripBounds(i), i < 2);
    auto whole = stripBounds(i);

    // faceplate body (below tab; extends over the sweep block on P1/P2)
    int faceTop = whole.getY() + 16;
    int faceBot = (i < 2 ? r.swShift.getBottom() : r.name.getBottom()) + 4;
    bevelOut(g, juce::Rectangle<int>(whole.getX(), faceTop, whole.getWidth(),
                                     faceBot - faceTop)
                    .toFloat(),
             t.faceA, t.faceB, 5.0f);

    // colored header tab
    auto tab = r.tab.toFloat();
    g.setGradientFill({kColor[i].brighter(0.1f), tab.getX(), tab.getY(),
                       kColor[i].darker(0.15f), tab.getX(), tab.getBottom(),
                       false});
    g.fillRoundedRectangle(tab, 4.0f);
    g.fillRect(tab.withTop(tab.getCentreY())); // square off bottom corners
    g.setColour(juce::Colours::white.withAlpha(0.30f));
    g.fillRect(tab.getX(), tab.getY(), tab.getWidth(), 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(px(6.5f));
    g.drawText(kShort[i], r.tab, juce::Justification::centred);

    // static ON/OFF LED for DMC + VRC6 P2/SAW (no own toggle)
    if (i == 4 || i == 6 || i == 7) {
      bevelIn(g, r.onoff.toFloat(), juce::Colour(0xff34332b), 3.0f);
      bool on = (i == 4) ? true : vrc6On;
      led(g, {(float)r.onoff.getX() + 9, (float)r.onoff.getCentreY()}, 4.5f,
          kColor[i], on);
      g.setColour(on ? juce::Colour(0xffe6e2d6) : juce::Colour(0xff76736b));
      g.setFont(px(6.0f));
      g.drawText(on ? "ON" : "OFF", r.onoff.withTrimmedLeft(20),
                 juce::Justification::centredLeft);
    }

    // static readout text
    if (juce::String(kStatic[i]).isNotEmpty()) {
      bevelIn(g, r.readout.toFloat(), juce::Colour(0xff201e18), 3.0f);
      g.setColour(kColor[i].brighter(0.6f));
      g.setFont(px(6.5f));
      g.drawText(kStatic[i], r.readout, juce::Justification::centred);
    }

    // DMC has no macro chip
    if (i == 4) {
      g.setColour(t.macroBg);
      g.fillRoundedRectangle(r.macro.toFloat(), 3.0f);
      g.setColour(juce::Colour(0xff86827a));
      g.setFont(px(6.0f));
      g.drawText("MAC  -", r.macro.reduced(5, 0),
                 juce::Justification::centredLeft);
    }

    // scope recessed well + frame (CRT glass drawn in paintOverChildren)
    bevelIn(g, r.scope.toFloat().expanded(1.5f), juce::Colour(0xff0d0d0c), 3.0f);

    // channel name footer
    g.setColour(t.nameText);
    g.setFont(px(5.0f));
    g.drawText(kName[i], r.name, juce::Justification::centred);
  }

  // VRC6 divider
  auto dl = stripBounds(5).getX() - 6;
  auto deckIn = deckBounds().reduced(18, 8);
  g.setGradientFill({juce::Colours::transparentBlack, (float)dl,
                     (float)deckIn.getY(), juce::Colour(0xff9b59b6), (float)dl,
                     (float)deckIn.getCentreY(), false});
  g.fillRect(dl, deckIn.getY(), 2, deckIn.getHeight());

  // ---- Footer ----
  g.setColour(juce::Colour(0xff6f7484));
  g.setFont(px(5.5f));
  g.drawText("v0.2.0  -  GPL-3.0  -  NTSC 1.789773 MHz", 0, kBaseH - 16, kBaseW,
             12, juce::Justification::centred);
}

void NessyAudioProcessorEditor::paintOverChildren(juce::Graphics &g) {
  for (int i = 0; i < kNumStrips; ++i)
    crtGlass(g, rowsFor(stripBounds(i), i < 2).scope.toFloat());

  // global scanlines
  g.setColour(juce::Colours::black.withAlpha(0.05f));
  for (int y = 0; y < kBaseH; y += 3)
    g.fillRect(0, y, kBaseW, 1);
}

// ---- Layout ---------------------------------------------------------------
void NessyAudioProcessorEditor::resizedContent() {
  // Header: volume dial (left of theme switch)
  auto ts = themeSwitchBounds();
  masterVolume->setBounds(ts.getX() - 70, 8, 60, 60);

  // Control rail cluster
  int y = 80, h = 30;
  voiceMode->setBounds(24, y, 132, h);
  arpToggle->setBounds(168, y, 52, h);
  arpPattern->setBounds(226, y, 92, h);
  arpOctaves->setBounds(324, y, 50, h);
  portamentoToggle->setBounds(386, y, 64, h);
  splitPoint->setBounds(462, y + 2, 130, h - 4);
  portamentoSpeed->setBounds(602, y + 2, 130, h - 4);

  // Channel strips
  for (int i = 0; i < kNumStrips; ++i) {
    auto r = rowsFor(stripBounds(i), i < 2);

    // ON/OFF toggle (P1 P2 TRI NSE -> own; VRC6 P1 -> vrc6Enable)
    gm::GmToggleButton *en = nullptr;
    if (i == 0) en = pulse1Toggle.get();
    else if (i == 1) en = pulse2Toggle.get();
    else if (i == 2) en = triangleToggle.get();
    else if (i == 3) en = noiseToggle.get();
    else if (i == 5) en = vrc6EnableToggle.get();
    if (en) en->setBounds(r.onoff);

    // readout-row control
    gm::ComboSelector *duty = nullptr;
    if (i == 0) duty = pulse1Duty.get();
    else if (i == 1) duty = pulse2Duty.get();
    else if (i == 5) duty = vrc6Pulse1Duty.get();
    else if (i == 6) duty = vrc6Pulse2Duty.get();
    if (duty) duty->setBounds(r.readout);
    if (i == 3) noiseModeToggle->setBounds(r.readout);

    // macro chip
    int m = macroIdx(i);
    if (m >= 0) macroBoxes[m]->setBounds(r.macro);

    // scope
    scopes[i]->setBounds(r.scope);

    // sweep (P1/P2)
    if (i < 2) {
      sweepEnables[i]->setBounds(r.swEnable);
      sweepDirs[i]->setBounds(r.swDir);
      sweepRates[i]->setBounds(r.swRate);
      sweepShifts[i]->setBounds(r.swShift);
    }
  }

  keyboard.setBounds(22, kBaseH - 18 - 60, kBaseW - 44, 58);
}

void NessyAudioProcessorEditor::mouseDown(const juce::MouseEvent &e) {
  auto segs = themeSegmentRects();
  for (int i = 0; i < 3; ++i)
    if (segs[i].contains(e.getPosition())) {
      setTheme(i);
      return;
    }
}
