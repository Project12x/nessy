#pragma once

#include "PluginProcessor.h"
#include "apu/NessyAPU.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>


// 8-bit Style Oscilloscope component
class ChannelOscilloscope : public juce::Component {
public:
  ChannelOscilloscope(int channelIdx, const NessyAPU &apu)
      : channel(channelIdx), apuRef(apu) {}

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    auto buffer = apuRef.getVisualizerBuffer(channel);

    if (buffer == nullptr)
      return;

    g.setColour(juce::Colours::white.withAlpha(0.6f));

    const int numSamples = NessyAPU::VISUALIZER_BUFFER_SIZE;
    const float midY = bounds.getCentreY();
    const float height = bounds.getHeight() * 0.8f;

    juce::Path p;
    bool started = false;

    // We only draw 64 segments to keep it "blocky/pixelated"
    const int steps = 64;
    const float xStep = bounds.getWidth() / (float)steps;

    for (int i = 0; i < steps; ++i) {
      int sampleIdx = (i * numSamples) / steps;
      float x = (float)i * xStep;
      float y = midY - (buffer[sampleIdx] * height * 0.5f);

      if (!started) {
        p.startNewSubPath(x, y);
        started = true;
      } else {
        p.lineTo(x, y);
      }
    }

    g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered,
                                         juce::PathStrokeType::butt));
  }

private:
  int channel;
  const NessyAPU &apuRef;
};

class NessyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer {
public:
  explicit NessyAudioProcessorEditor(NessyAudioProcessor &);
  ~NessyAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override { repaint(); }
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

  juce::Image backgroundImage;

  // Oscilloscopes
  std::vector<std::unique_ptr<ChannelOscilloscope>> oscilloscopes;

  // Macro preset combo boxes (one per channel)
  juce::ComboBox macroBoxes[7];
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      macroAttachments[7];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessorEditor)
};
