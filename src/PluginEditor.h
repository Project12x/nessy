#pragma once

#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class NessyAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit NessyAudioProcessorEditor(NessyAudioProcessor &);
  ~NessyAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  NessyAudioProcessor &processorRef;

  // Virtual keyboard for standalone testing
  juce::MidiKeyboardComponent keyboard;

  // Master volume
  juce::Slider masterVolumeSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      masterVolumeAttachment;

  // Channel enable toggles
  juce::ToggleButton pulse1Toggle{"P1"};
  juce::ToggleButton pulse2Toggle{"P2"};
  juce::ToggleButton triangleToggle{"TRI"};
  juce::ToggleButton noiseToggle{"NSE"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      pulse1Attachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      pulse2Attachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      triangleAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      noiseAttachment;

  // Duty cycle selectors
  juce::ComboBox pulse1DutyBox;
  juce::ComboBox pulse2DutyBox;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      pulse1DutyAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      pulse2DutyAttachment;

  // Voice mode selector
  juce::ComboBox voiceModeBox;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      voiceModeAttachment;

  // Split point slider (for Pitch-Split mode)
  juce::Slider splitPointSlider;
  juce::Label splitPointLabel{"", "Split"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      splitPointAttachment;

  // Noise mode toggle
  juce::ToggleButton noiseModeToggle{"Short"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      noiseModeAttachment;

  // VRC6 Expansion controls
  juce::ToggleButton vrc6EnableToggle{"VRC6"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      vrc6EnableAttachment;

  juce::ComboBox vrc6Pulse1DutyBox;
  juce::ComboBox vrc6Pulse2DutyBox;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      vrc6Pulse1DutyAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      vrc6Pulse2DutyAttachment;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
