#pragma once

// IVoiceSink: the minimal boundary VoiceAllocator uses to drive sound. NessyAPU
// implements it (it already has these exact methods); tests use a mock. This
// keeps VoiceAllocator free of any JUCE/NSFPlay dependency so it unit-tests in
// isolation. GPL-3.0.

namespace nessy {

class IVoiceSink {
public:
  virtual ~IVoiceSink() = default;

  // Match NessyAPU's existing signatures exactly.
  virtual void noteOn(int channel, int midiNote, float velocity) = 0;
  virtual void noteOff(int channel) = 0;
  virtual bool isChannelEnabled(int channel) const = 0;
};

} // namespace nessy
