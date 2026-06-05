#pragma once

#include "PluginProcessor.h"
#include "apu/NessyAPU.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>

// ghostmoon UI catalog
#include <Knob.h>
#include <GmToggleButton.h>
#include <ComboSelector.h>
#include <HSlider.h>
#include <Oscilloscope.h>
#include <ScaledEditor.h>

// ---------------------------------------------------------------------------
// NessyTheme — one hardware skin (NES / Famicom / FDS).
// Tokens mirror nessy.css :root + data-theme variables (see DESIGN.md).
// Paint chrome reads from the active theme; gm components are recoloured via
// per-instance setColour on each theme switch (ThemeManager is left untouched).
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

class NessyAudioProcessorEditor : public gm::ScaledEditor, private juce::Timer {
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
  void setTheme(int index);     // swap palette, re-skin gm components, repaint
  void applyThemeToControls();  // push current theme colours into gm components
  const NessyTheme &theme() const { return currentTheme; }

  // Geometry shared by paint() + resizedContent()
  juce::Rectangle<int> headerBounds() const;
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

  NessyAudioProcessor &processorRef;

  NessyTheme currentTheme;
  int themeIndex = 0;

  // --- Controls (APVTS-bound gm components) ---
  std::unique_ptr<gm::Knob> masterVolume;

  // Per-strip enable toggles (P1 P2 TRI NSE have params; DMC/VRC6 handled below)
  std::unique_ptr<gm::GmToggleButton> pulse1Toggle, pulse2Toggle,
      triangleToggle, noiseToggle;
  std::unique_ptr<gm::GmToggleButton> vrc6EnableToggle; // gates all 3 VRC6 strips

  // Readout-row controls
  std::unique_ptr<gm::ComboSelector> pulse1Duty, pulse2Duty;
  std::unique_ptr<gm::ComboSelector> vrc6Pulse1Duty, vrc6Pulse2Duty;
  std::unique_ptr<gm::GmToggleButton> noiseModeToggle; // NSE readout (Long/Short)

  // Macro chips — index order P1 P2 TRI NSE VRC6P1 VRC6P2 SAW (DMC has none)
  std::array<std::unique_ptr<gm::ComboSelector>, 7> macroBoxes;

  // Global cluster: Voice/Arp/Porta/Split are the painted gamepad (header);
  // the granular controls live in the rail.
  std::unique_ptr<gm::ComboSelector> arpPattern, arpOctaves;
  std::unique_ptr<gm::HSlider> splitPoint, portamentoSpeed;

  // Hardware sweep, woven into the P1/P2 strips
  std::array<std::unique_ptr<gm::GmToggleButton>, 2> sweepEnables;
  std::array<std::unique_ptr<gm::ComboSelector>, 2> sweepDirs, sweepRates,
      sweepShifts;

  // Per-strip oscilloscopes (strip index == NessyAPU channel index)
  std::array<std::unique_ptr<gm::Oscilloscope>, kNumStrips> scopes;

  juce::MidiKeyboardComponent keyboard;
  juce::Image backgroundImage;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
