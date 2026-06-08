#pragma once

#include "PluginProcessor.h"
#include "NessyUI.h" // Nessy NES LookAndFeel + scope (replaces proprietary gm:: widgets)
#include "apu/NessyAPU.h"
#include <ghostmoon/ui/ScaledEditor.h> // gm::ui::ScaledEditor (ghostmoon-oss, MIT)
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>
#include <vector>

class NsfPlayerWindow; // forward declaration — defined in NsfPlayerWindow.h

// ---------------------------------------------------------------------------
// NessyTheme — one hardware skin (NES / Famicom / FDS).
// Tokens mirror nessy.css :root + data-theme variables (see DESIGN.md).
// Paint chrome reads from the active theme; juce controls are recoloured via
// per-instance setColour on each theme switch.
// ---------------------------------------------------------------------------
struct NessyTheme {
  juce::String name;        // "NES" / "FC" / "FDS"
  juce::String subtitle;    // header subtitle line

  juce::Colour hdrA, hdrB, hdrDim;
  juce::Colour stripe, wordmark, subtitleCol;
  juce::Colour padShellA, padShellB, padPlateA, padPlateB, padAccent;
  juce::Colour padAbHi, padAb, padAbLo;
  juce::Colour railA, railB, chipA, chipB;
  juce::Colour faceA, faceB, faceBorder, macroBg;
  juce::Colour nameText, railText;
  juce::Colour trayA, trayB;

  static NessyTheme nes();
  static NessyTheme famicom();
  static NessyTheme fds();
  static NessyTheme byIndex(int i); // 0=NES 1=FC 2=FDS
};

class NessyAudioProcessorEditor : public gm::ui::ScaledEditor, private juce::Timer {
public:
  explicit NessyAudioProcessorEditor(NessyAudioProcessor &);
  ~NessyAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void paintOverChildren(juce::Graphics &) override; // CRT glass + scanlines
  void resizedContent() override;
  void mouseDown(const juce::MouseEvent &) override;

  static constexpr int kBaseW = 1040;
  static constexpr int kBaseH = 508;
  static constexpr int kNumStrips = 8; // P1 P2 TRI NSE DMC | VRC6 P1 P2 SAW

private:
  void timerCallback() override;

  // Theme handling
  void setTheme(int index);     // swap palette, re-skin controls, repaint
  void applyThemeToControls();  // push current theme colours into juce controls
  const NessyTheme &theme() const { return currentTheme; }

  // Geometry shared by paint() + resizedContent()
  juce::Rectangle<int> headerBounds() const;
  juce::Rectangle<int> cartridgeBounds() const; // decorative preset loader row
  juce::Rectangle<int> deckBounds() const;
  juce::Rectangle<int> stripBounds(int strip) const;   // outer faceplate
  juce::Rectangle<int> themeSwitchBounds() const;
  std::array<juce::Rectangle<int>, 3> themeSegmentRects() const;

  // Gamepad cluster (header) — painted + hit-tested, drives Voice/Arp/Porta/Split
  struct GpadRegions {
    juce::Rectangle<int> cluster, dpad, voice, arp, aBtn, bBtn;
  };
  GpadRegions gamepadRegions() const;
  void drawGamepad(juce::Graphics &g);
  void drawCartridge(juce::Graphics &g); // decorative preset loader (Phase 11 wires it)

  NessyAudioProcessor &processorRef;
  nessy::NessyLookAndFeel lnf; // declared first → destroyed after the controls

  NessyTheme currentTheme;
  int themeIndex = 0;

  // --- Controls: stock juce widgets styled by the NES LookAndFeel ---
  // Channel ON/OFF, NSE Long/Short, and the P1/P2 SWEEP enables are stylized
  // PAINTED toggles (channel-coloured LED + label), drawn in paint() and
  // hit-tested in mouseDown — not widgets.

  std::unique_ptr<juce::Slider> masterVolume;            // rotary
  std::unique_ptr<juce::ComboBox> pulse1Duty, pulse2Duty;
  std::unique_ptr<juce::ComboBox> vrc6Pulse1Duty, vrc6Pulse2Duty;
  std::array<std::unique_ptr<juce::ComboBox>, 7> macroBoxes; // P1 P2 TRI NSE VP1 VP2 SAW (no DMC)
  std::unique_ptr<juce::ComboBox> arpPattern, arpOctaves;
  std::unique_ptr<juce::Slider> splitPoint, portamentoSpeed; // linear
  std::array<std::unique_ptr<juce::ComboBox>, 2> sweepDirs, sweepRates, sweepShifts;

  // Per-strip oscilloscopes (strip index == NessyAPU channel index)
  std::array<std::unique_ptr<nessy::NessyScope>, kNumStrips> scopes;

  // APVTS attachments (keep alive for the controls' lifetime)
  std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
  std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;

  juce::MidiKeyboardComponent keyboard;
  juce::Image backgroundImage;

  // --- NSF load UI state ---
  juce::Rectangle<int> m_ejectBounds, m_nsfPrevBounds, m_nsfNextBounds, m_cartBodyBounds;
  // m_nsfChooser removed: file loading is now owned by NsfPlayerWindow's LOAD button.
  int m_uiSong = 0;

  // --- NSF player floating window ---
  std::unique_ptr<NsfPlayerWindow> m_nsfWindow;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
