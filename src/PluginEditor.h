#pragma once

#include "PluginProcessor.h"
#include "apu/NessyAPU.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>

// ghostmoon UI catalog
#include <Knob.h>
#include <GmToggleButton.h>
#include <ComboSelector.h>
#include <HSlider.h>
#include <Oscilloscope.h>

class NessyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer {
public:
  explicit NessyAudioProcessorEditor(NessyAudioProcessor &);
  ~NessyAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;
  NessyAudioProcessor &processorRef;

  // Virtual keyboard
  juce::MidiKeyboardComponent keyboard;

  // Master volume
  std::unique_ptr<gm::Knob> masterVolume;

  // Channel enable toggles (P1, P2, TRI, NSE)
  std::unique_ptr<gm::GmToggleButton> pulse1Toggle;
  std::unique_ptr<gm::GmToggleButton> pulse2Toggle;
  std::unique_ptr<gm::GmToggleButton> triangleToggle;
  std::unique_ptr<gm::GmToggleButton> noiseToggle;

  // Duty cycle selectors
  std::unique_ptr<gm::ComboSelector> pulse1Duty;
  std::unique_ptr<gm::ComboSelector> pulse2Duty;

  // Voice mode
  std::unique_ptr<gm::ComboSelector> voiceMode;

  // Split point
  std::unique_ptr<gm::HSlider> splitPoint;

  // Noise mode
  std::unique_ptr<gm::GmToggleButton> noiseModeToggle;

  // Portamento
  std::unique_ptr<gm::GmToggleButton> portamentoToggle;
  std::unique_ptr<gm::HSlider> portamentoSpeed;

  // VRC6 Expansion
  std::unique_ptr<gm::GmToggleButton> vrc6EnableToggle;
  std::unique_ptr<gm::ComboSelector> vrc6Pulse1Duty;
  std::unique_ptr<gm::ComboSelector> vrc6Pulse2Duty;

  // Macro preset selectors (one per channel: P1, P2, TRI, NSE, VRC6_P1, VRC6_P2, VRC6_SAW)
  std::unique_ptr<gm::ComboSelector> macroBoxes[7];

  // Hardware Sweep (Pulse 1 & 2)
  std::unique_ptr<gm::GmToggleButton> sweepEnables[2];
  std::unique_ptr<gm::ComboSelector> sweepDirs[2];
  std::unique_ptr<gm::ComboSelector> sweepRates[2];
  std::unique_ptr<gm::ComboSelector> sweepShifts[2];

  // Arpeggiator
  std::unique_ptr<gm::GmToggleButton> arpToggle;
  std::unique_ptr<gm::ComboSelector> arpPattern;
  std::unique_ptr<gm::ComboSelector> arpOctaves;

  // Oscilloscopes (P1, P2, TRI, NSE, VRC6_P1, VRC6_P2, VRC6_SAW)
  std::unique_ptr<gm::Oscilloscope> scopes[7];

  // Background image
  juce::Image backgroundImage;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
