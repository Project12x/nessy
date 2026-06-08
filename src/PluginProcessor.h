#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include <vector>

// ghostmoon DSP utilities
#include <ghostmoon/SafetyLimiter.h>
#include <ghostmoon/DCBlocker.h>
#include <ghostmoon/CpuMeter.h>
#include <ghostmoon/ParamSmoother.h>

// NSF engine (PIMPL — safe alongside JUCE headers, no NSFPlay macro leakage)
#include "nsf/NsfEngine.h"

class NessyAPU;
class VoiceAllocator;

class NessyAudioProcessor : public juce::AudioProcessor {
public:
  NessyAudioProcessor();
  ~NessyAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  // Access to keyboard state for UI
  juce::MidiKeyboardState &getKeyboardState() { return keyboardState; }

  // Access to APVTS for UI controls
  juce::AudioProcessorValueTreeState &getAPVTS() { return parameters; }

  // Access to APU for UI oscilloscopes
  NessyAPU *getAPU() { return apu.get(); }

  // CPU load for diagnostics
  float getCpuLoad() const { return cpuMeter.getLoadPercent(); }

  // --- NSF playback API (call from message thread only) ---
  void loadNsf(const uint8_t* data, size_t size, int song = 0);
  void selectNsfSong(int song);
  void setPlaybackMode(bool nsf);
  bool isNsfMode() const { return m_playbackMode.load(std::memory_order_relaxed) == 1; }
  juce::String getNsfTitle() const;
  juce::String getNsfArtist()    const { return m_nsfArtist; }
  juce::String getNsfCopyright() const { return m_nsfCopyright; }
  juce::String getNsfChips()     const { return m_nsfChips; }
  int getNsfSongCount() const;
  int getNsfSong() const { return m_nsfSong; }

  // NSF transport: pause/resume without restarting the engine.
  void setNsfPlaying(bool p) { m_nsfPlaying.store(p, std::memory_order_release); }
  bool isNsfPlaying() const  { return m_nsfPlaying.load(std::memory_order_relaxed); }

  // Drain the retire slot (call from message thread, e.g. editor timer, to free old engines).
  void retireOldEngine();

private:
  // Audio parameters
  juce::AudioProcessorValueTreeState parameters;

  // NES APU emulation
  std::unique_ptr<NessyAPU> apu;

  // Voice allocator
  std::unique_ptr<VoiceAllocator> voiceAllocator;

  // Keyboard state for standalone virtual keyboard
  juce::MidiKeyboardState keyboardState;

  // Current sample rate
  double currentSampleRate = 44100.0;

  // Last MIDI velocity for arpeggiator dispatch
  float lastVelocity = 0.8f;

  // Live channel-enable sync (change detection): P1, P2, TRI, NSE, DMC
  bool m_chEnable[5] = {true, true, true, true, true};

  // ghostmoon DSP chain
  gm::SafetyLimiter safetyLimiter;
  gm::DCBlocker dcBlockerL, dcBlockerR;
  gm::CpuMeter cpuMeter;
  gm::ParamSmoother<float> volumeSmoother{0.02f};

  // --- NSF playback state ---
  // 0 = Synth, 1 = NSF. Readable from both threads; written by message thread only.
  std::atomic<int> m_playbackMode { 0 };

  // Lock-free engine handoff (message thread -> audio thread).
  // loadNsf() publishes here; processBlock() adopts atomically.
  std::atomic<nessy::NsfEngine*> m_pendingNsf { nullptr };

  // Lock-free retire slot (audio thread -> message thread).
  // processBlock() deposits retired engines here; retireOldEngine() deletes them.
  // RT-safety contract: loadNsf() calls retireOldEngine() before publishing,
  // ensuring the slot is empty when the audio thread next deposits a stale engine.
  std::atomic<nessy::NsfEngine*> m_retireNsf { nullptr };

  // Audio-thread-owned active engine. Mutated only inside processBlock().
  std::unique_ptr<nessy::NsfEngine> m_activeNsf;

  // Pre-allocated scratch for mono int16 render (sized in prepareToPlay).
  std::vector<int16_t> m_nsfScratch;

  // Cached NSF bytes for selectNsfSong().
  std::vector<uint8_t> m_nsfData;

  // Active song index (message-thread-written, used on rebuild in selectNsfSong).
  int m_nsfSong { 0 };

  // Cached metadata set in loadNsf() from the new engine before publishing,
  // safe to read from the message thread without touching m_activeNsf.
  juce::String m_nsfTitle;
  juce::String m_nsfArtist;
  juce::String m_nsfCopyright;
  juce::String m_nsfChips;
  int m_nsfSongCount { 0 };

  // NSF transport: true = render audio; false = output silence (freeze/pause).
  // Written by message thread; read by audio thread (relaxed load, release store).
  std::atomic<bool> m_nsfPlaying { true };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NessyAudioProcessor)
};
