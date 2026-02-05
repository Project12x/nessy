#pragma once

#include "PluginProcessor.h"

class NessyAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit NessyAudioProcessorEditor(NessyAudioProcessor &);
  ~NessyAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  NessyAudioProcessor &processorRef;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
