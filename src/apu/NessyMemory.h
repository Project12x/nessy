#pragma once

#include "nsfplay/xgm/devices/device.h"
#include <map>
#include <vector>


// Virtual memory device for the DMC channel ($8000-$FFFF)
// Hosts DPCM samples for playback
class NessyMemory : public xgm::IDevice {
public:
  NessyMemory() {
    // Initialize 32KB of RAM (0x8000 - 0xFFFF)
    m_memory.resize(0x8000, 0x00);

    // Load factory samples
    loadFactorySamples();
  }

  void Reset() override {
    // Memory persistence? Or clear on reset?
    // Generally ROM doesn't clear on reset.
  }

  bool Write(xgm::UINT32 adr, xgm::UINT32 val, xgm::UINT32 id = 0) override {
    if (adr >= 0x8000 && adr <= 0xFFFF) {
      m_memory[adr - 0x8000] = static_cast<uint8_t>(val);
      return true;
    }
    return false;
  }

  bool Read(xgm::UINT32 adr, xgm::UINT32 &val, xgm::UINT32 id = 0) override {
    if (adr >= 0x8000 && adr <= 0xFFFF) {
      val = m_memory[adr - 0x8000];
      return true;
    }
    return false;
  }

  void loadSample(uint16_t startAddress, const std::vector<uint8_t> &data) {
    if (startAddress < 0x8000)
      return;

    size_t offset = startAddress - 0x8000;
    for (size_t i = 0; i < data.size(); ++i) {
      if (offset + i < m_memory.size()) {
        m_memory[offset + i] = data[i];
      }
    }
  }

private:
  std::vector<uint8_t> m_memory;

  void loadFactorySamples() {
    // Create a simple "Kick" sample at 0xC000
    // 1-bit delta encoded.
    // 0xAA = 10101010 (down, down, down, down) - wait, bit 0 is LSB.
    // DPCM: 1=up, 0=down.

    // Let's make a triangle wave shape ~
    // UP: 11111111 (0xFF)
    // DOWN: 00000000 (0x00)

    std::vector<uint8_t> kickSample;
    // Attack
    for (int i = 0; i < 32; ++i)
      kickSample.push_back(0xFF);
    // Decay
    for (int i = 0; i < 32; ++i)
      kickSample.push_back(0x00);

    loadSample(0xC000, kickSample);
  }
};
