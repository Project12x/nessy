#pragma once

// NessyMemory: Virtual NES address space ($8000-$FFFF) for hosting DPCM samples.
// Implements xgm::IDevice so NES_DMC can read sample bytes directly.
// GPL-3.0

#include "nsfplay/xgm/devices/device.h"
#include <cstdint>
#include <vector>

// DMC register encoding helpers
// Sample address: $C000 + (reg * $40)
// Sample length:  (reg * $10) + 1 bytes
struct DmcSample {
  uint16_t address;   // start address in NES space ($8000-$FFFF)
  uint8_t  addrReg;   // value for $4012 = (address - $C000) >> 6
  uint8_t  lenReg;    // value for $4013 = (byteCount - 1) >> 4
  uint8_t  rateReg;   // value for $4010 (0-F, F = highest/fastest pitch)
};

// MIDI note → sample slot mapping (General MIDI drum map subset)
enum DmcSlot {
  SLOT_KICK    = 0,  // MIDI 36 (C2)
  SLOT_SNARE   = 1,  // MIDI 38 (D2)
  SLOT_HIHAT_C = 2,  // MIDI 42 (F#2) closed
  SLOT_HIHAT_O = 3,  // MIDI 46 (Bb2) open
  SLOT_TOM_LO  = 4,  // MIDI 41 (F2)
  SLOT_TOM_HI  = 5,  // MIDI 45 (A2)
  NUM_DMC_SLOTS
};

class NessyMemory : public xgm::IDevice {
public:
  NessyMemory() {
    m_memory.resize(0x8000, 0x00); // 32KB: $8000-$FFFF
    buildFactorySamples();
  }

  void Reset() override {}

  bool Write(xgm::UINT32 adr, xgm::UINT32 val,
             xgm::UINT32 /*id*/ = 0) override {
    if (adr >= 0x8000 && adr <= 0xFFFF) {
      m_memory[adr - 0x8000] = static_cast<uint8_t>(val);
      return true;
    }
    return false;
  }

  bool Read(xgm::UINT32 adr, xgm::UINT32 &val,
            xgm::UINT32 /*id*/ = 0) override {
    if (adr >= 0x8000 && adr <= 0xFFFF) {
      val = m_memory[adr - 0x8000];
      return true;
    }
    return false;
  }

  // Returns sample descriptor for a given slot, or nullptr if invalid.
  const DmcSample* getSample(int slot) const {
    if (slot < 0 || slot >= NUM_DMC_SLOTS) return nullptr;
    return &m_samples[slot];
  }

  // Map a MIDI note number to a DMC slot index (-1 if unmapped)
  static int midiToDmcSlot(int midiNote) {
    switch (midiNote) {
    case 35: case 36: return SLOT_KICK;      // B1 / C2
    case 38: case 40: return SLOT_SNARE;     // D2 / E2
    case 42: case 44: return SLOT_HIHAT_C;   // F#2 / G#2
    case 46:          return SLOT_HIHAT_O;   // Bb2
    case 41: case 43: return SLOT_TOM_LO;    // F2 / G2
    case 45: case 47: return SLOT_TOM_HI;    // A2 / B2
    default:          return SLOT_KICK;      // fallback
    }
  }

private:
  std::vector<uint8_t> m_memory;
  DmcSample m_samples[NUM_DMC_SLOTS];

  void storeSample(int slot, uint16_t address, const std::vector<uint8_t>& data,
                   uint8_t rateReg) {
    // Write bytes into virtual memory
    uint16_t offset = address - 0x8000;
    for (size_t i = 0; i < data.size() && (offset + i) < m_memory.size(); ++i)
      m_memory[offset + i] = data[i];

    // Build descriptor: addrReg = (addr - 0xC000) >> 6, lenReg = (len-1) >> 4
    uint8_t addrReg = static_cast<uint8_t>((address - 0xC000) >> 6);
    uint8_t lenReg  = static_cast<uint8_t>((static_cast<int>(data.size()) - 1) >> 4);
    m_samples[slot] = { address, addrReg, lenReg, rateReg };
  }

  // ---------------------------------------------------------------------------
  // Factory DPCM patterns
  // Each byte = 8 delta steps: bit 1 = up, bit 0 = down (LSB first)
  // 0xFF = 8 ups, 0x00 = 8 downs, 0xAA = alternating
  // ---------------------------------------------------------------------------
  void buildFactorySamples() {
    // --- KICK ($C000): descending thump ---
    // Strong attack (up), then rapid fall
    std::vector<uint8_t> kick;
    for (int i = 0; i < 8;  ++i) kick.push_back(0xFF); // heavy attack
    for (int i = 0; i < 8;  ++i) kick.push_back(0x7F); // partial fall
    for (int i = 0; i < 8;  ++i) kick.push_back(0x00); // hard bottom
    for (int i = 0; i < 8;  ++i) kick.push_back(0x55); // settle
    storeSample(SLOT_KICK, 0xC000, kick, 0x0A); // rate A (~comfortable)

    // --- SNARE ($C100): noisy burst ---
    // Pattern alternates strongly to create broadband noise
    std::vector<uint8_t> snare;
    for (int i = 0; i < 8;  ++i) snare.push_back(0xFF);
    for (int i = 0; i < 16; ++i) snare.push_back(i % 2 ? 0x6D : 0x92); // noise
    for (int i = 0; i < 8;  ++i) snare.push_back(0x00);
    storeSample(SLOT_SNARE, 0xC100, snare, 0x0C); // rate C

    // --- CLOSED HI-HAT ($C200): very short click ---
    std::vector<uint8_t> hihatC;
    for (int i = 0; i < 4;  ++i) hihatC.push_back(0xFF);
    for (int i = 0; i < 4;  ++i) hihatC.push_back(0xAA);
    for (int i = 0; i < 4;  ++i) hihatC.push_back(0x00);
    storeSample(SLOT_HIHAT_C, 0xC200, hihatC, 0x0F); // rate F (fastest)

    // --- OPEN HI-HAT ($C300): longer shimmer ---
    std::vector<uint8_t> hihatO;
    for (int i = 0; i < 4;  ++i) hihatO.push_back(0xFF);
    for (int i = 0; i < 24; ++i) hihatO.push_back(i % 3 == 0 ? 0x6B : 0x94);
    for (int i = 0; i < 4;  ++i) hihatO.push_back(0x55);
    storeSample(SLOT_HIHAT_O, 0xC300, hihatO, 0x0E); // rate E

    // --- LOW TOM ($C400): mid thump ---
    std::vector<uint8_t> tomLo;
    for (int i = 0; i < 6;  ++i) tomLo.push_back(0xFF);
    for (int i = 0; i < 10; ++i) tomLo.push_back(0x33);
    for (int i = 0; i < 6;  ++i) tomLo.push_back(0x00);
    storeSample(SLOT_TOM_LO, 0xC400, tomLo, 0x09); // rate 9 (lower pitch)

    // --- HIGH TOM ($C500): higher thump ---
    std::vector<uint8_t> tomHi;
    for (int i = 0; i < 5;  ++i) tomHi.push_back(0xFF);
    for (int i = 0; i < 8;  ++i) tomHi.push_back(0x55);
    for (int i = 0; i < 5;  ++i) tomHi.push_back(0x00);
    storeSample(SLOT_TOM_HI, 0xC500, tomHi, 0x0B); // rate B
  }
};
