#pragma once

// Test double for IVoiceSink: records the sequence of noteOn/noteOff channel
// ids the allocator drives, and answers isChannelEnabled from a mask.

#include "IVoiceSink.h"
#include <set>
#include <vector>

struct MockVoiceSink : public nessy::IVoiceSink {
  struct OnEvent { int channel; int note; float velocity; };

  std::vector<OnEvent> ons;        // every noteOn, in order
  std::vector<int>     offs;       // every noteOff channel, in order
  std::set<int>        disabled;   // channels reported disabled

  void noteOn(int channel, int midiNote, float velocity) override {
    ons.push_back({channel, midiNote, velocity});
  }
  void noteOff(int channel) override { offs.push_back(channel); }
  bool isChannelEnabled(int channel) const override {
    return disabled.find(channel) == disabled.end();
  }

  // Helpers for assertions.
  std::vector<int> onChannels() const {
    std::vector<int> v;
    for (auto& e : ons) v.push_back(e.channel);
    return v;
  }
  int lastOnChannelFor(int note) const {
    for (auto it = ons.rbegin(); it != ons.rend(); ++it)
      if (it->note == note) return it->channel;
    return -1;
  }
  void clear() { ons.clear(); offs.clear(); }
};
