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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
