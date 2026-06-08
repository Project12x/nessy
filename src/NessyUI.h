#pragma once

// Nessy-owned NES "Front-Loader" UI layer — replaces the proprietary ghostmoon
// gm:: control widgets with stock juce:: controls styled by a Nessy LookAndFeel,
// plus a scope component that draws the skin-neutral gm::ui::Oscilloscope (from
// ghostmoon-oss) in Nessy's own NES skin. GPL-3.0 (part of the Nessy plugin).

#include <juce_gui_basics/juce_gui_basics.h>
#include <ghostmoon/ui/Oscilloscope.h>
#include "BinaryData.h"

namespace nessy {

// ---------------------------------------------------------------------------
// Shared chrome paint helpers — used by PluginEditor and NsfPlayerWindow.
// These replicate the NES "Front-Loader" plastic/PCB look.
// ---------------------------------------------------------------------------

inline void vgrad(juce::Graphics& g, juce::Rectangle<float> r,
                  juce::Colour a, juce::Colour b, float radius) {
  g.setGradientFill({a, r.getX(), r.getY(), b, r.getX(), r.getBottom(), false});
  if (radius > 0) g.fillRoundedRectangle(r, radius); else g.fillRect(r);
}

inline void bevelOut(juce::Graphics& g, juce::Rectangle<float> r,
                     juce::Colour a, juce::Colour b, float radius) {
  g.setColour(juce::Colours::black.withAlpha(0.25f));
  g.fillRoundedRectangle(r.translated(0, 1.5f), radius);
  vgrad(g, r, a, b, radius);
  g.setColour(juce::Colours::white.withAlpha(0.40f));
  g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
  g.setColour(juce::Colours::black.withAlpha(0.16f));
  g.fillRect(juce::Rectangle<float>(r.getX() + radius, r.getBottom() - 2.0f,
                                    r.getWidth() - 2 * radius, 1.4f));
}

inline void bevelIn(juce::Graphics& g, juce::Rectangle<float> r,
                    juce::Colour fill, float radius) {
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

inline void led(juce::Graphics& g, juce::Point<float> c, float rad,
                juce::Colour col, bool on) {
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

// CRT glass overlay for a scope rect.
inline void crtGlass(juce::Graphics& g, juce::Rectangle<float> r) {
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

// Press Start 2P pixel font (embedded in NessyFonts BinaryData).
inline juce::Font pixelFont(float h) {
  static auto tf = juce::Typeface::createSystemTypefaceFor(
      BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize);
  return juce::Font(juce::FontOptions(tf).withHeight(h));
}

// ---------------------------------------------------------------------------
// NessyScope — juce::Component wrapping a gm::ui::Oscilloscope (ghostmoon-oss).
// Draws a dark CRT face + the channel-coloured trace; the editor lays the
// recessed well behind it and the CRT-glass overlay on top.
// ---------------------------------------------------------------------------
class NessyScope : public juce::Component {
public:
  NessyScope() { setInterceptsMouseClicks(false, false); }

  // Audio thread.
  void process(const float* samples, int numSamples) {
    osc_.process(samples, numSamples);
  }
  void setTraceColour(juce::Colour c) { trace_ = c; }

  void paint(juce::Graphics& g) override {
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff161614));
    g.fillRect(b);
    // faint center-cross graticule (CRT phosphor grid)
    g.setColour(juce::Colour(0xff5a5a52).withAlpha(0.16f));
    g.drawHorizontalLine((int)b.getCentreY(), b.getX(), b.getRight());
    g.drawVerticalLine((int)b.getCentreX(), b.getY(), b.getBottom());
    auto path = osc_.makePath(b.reduced(2.0f), 0.42f);
    g.setColour(trace_.withAlpha(0.12f)); // wide phosphor bloom
    g.strokePath(path, juce::PathStrokeType(5.5f));
    g.setColour(trace_.withAlpha(0.30f)); // inner glow
    g.strokePath(path, juce::PathStrokeType(3.0f));
    g.setColour(trace_); // sharp trace
    g.strokePath(path, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
  }

private:
  gm::ui::Oscilloscope osc_;
  juce::Colour trace_{0xff33ed33};
};

// ---------------------------------------------------------------------------
// NessyLookAndFeel — NES skin for the stock juce:: controls used in the editor:
// the volume Knob (rotary Slider), the duty/macro/arp ComboBoxes, and the
// split/glide linear Sliders. Colours are read per-instance via findColour so
// the editor can re-skin them on a theme switch.
// ---------------------------------------------------------------------------
class NessyLookAndFeel : public juce::LookAndFeel_V4 {
public:
  NessyLookAndFeel() {
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff201e18));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe6e2d6));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff3a382e));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  }

  // --- Volume dial ---
  void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                        float pos, float startAngle, float endAngle,
                        juce::Slider& s) override {
    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float angle = startAngle + pos * (endAngle - startAngle);
    auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);

    // knurled outer ring + 11 radiating tick lines (mockup FL_Dial)
    g.setColour(juce::Colour(0xff2a2a28));
    g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
    g.setColour(juce::Colour(0xffcfcabd).withAlpha(0.55f));
    for (int i = 0; i < 11; ++i) {
      float a = (-135.0f + i * 27.0f) * juce::MathConstants<float>::pi / 180.0f;
      g.drawLine(cx + std::cos(a) * radius, cy + std::sin(a) * radius,
                 cx + std::cos(a) * (radius - 3.0f), cy + std::sin(a) * (radius - 3.0f),
                 1.1f);
    }
    // cap
    float capR = radius * 0.72f;
    juce::ColourGradient cap(juce::Colour(0xff3a3a36), cx - capR * 0.4f, cy - capR * 0.4f,
                             juce::Colour(0xff15151a), cx + capR, cy + capR, true);
    g.setGradientFill(cap);
    g.fillEllipse(cx - capR, cy - capR, capR * 2, capR * 2);
    g.setColour(juce::Colours::black);
    g.drawEllipse(cx - capR, cy - capR, capR * 2, capR * 2, 1.0f);
    // pointer
    juce::Point<float> tip(cx + std::cos(angle) * capR * 0.85f,
                           cy + std::sin(angle) * capR * 0.85f);
    g.setColour(accent.withAlpha(0.4f));
    g.drawLine({{cx, cy}, tip}, 4.0f);
    g.setColour(accent);
    g.drawLine({{cx, cy}, tip}, 2.0f);
  }

  // --- Combo boxes (duty / macro / arp pattern / octaves) ---
  juce::Font getComboBoxFont(juce::ComboBox&) override { return pixelFont(6.5f); }
  juce::Font getPopupMenuFont() override { return pixelFont(7.0f); }

  void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int,
                    juce::ComboBox& box) override {
    auto b = juce::Rectangle<float>(0, 0, (float)w, (float)h);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(b, 3.0f);
    // top inner shadow
    g.setGradientFill({juce::Colours::black.withAlpha(0.4f), b.getX(), b.getY(),
                       juce::Colours::transparentBlack, b.getX(), b.getY() + h * 0.5f, false});
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);
    // arrow
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    juce::Path arrow;
    float ax = w - 9.0f, ay = h * 0.5f;
    arrow.addTriangle(ax - 3, ay - 1.6f, ax + 3, ay - 1.6f, ax, ay + 2.4f);
    g.fillPath(arrow);
  }

  void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
    label.setBounds(4, 0, box.getWidth() - 14, box.getHeight());
    label.setFont(pixelFont(6.5f));
    label.setJustificationType(juce::Justification::centredLeft);
  }

  // --- Linear sliders (split / glide) ---
  void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                        float, float, juce::Slider::SliderStyle,
                        juce::Slider& s) override {
    auto track = juce::Rectangle<float>((float)x, y + h * 0.5f - 3.0f, (float)w, 6.0f);
    g.setColour(s.findColour(juce::Slider::backgroundColourId));
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
    // fill
    g.setColour(s.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(track.withWidth(juce::jmax(0.0f, pos - x)), 3.0f);
    // thumb
    float tw = 9.0f;
    juce::Rectangle<float> thumb(pos - tw * 0.5f, (float)y, tw, (float)h);
    juce::ColourGradient tg(juce::Colour(0xffd2cec3), thumb.getX(), thumb.getY(),
                            juce::Colour(0xff5a5a52), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill(tg);
    g.fillRoundedRectangle(thumb, 2.5f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(thumb, 2.5f, 1.0f);
  }
};

} // namespace nessy
