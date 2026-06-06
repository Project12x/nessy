#include <catch2/catch_test_macros.hpp>
#include "ChipSmoke.h"
#include "xgm/devices/Sound/nes_mmc5.h"
#include "xgm/devices/Sound/nes_fds.h"
#include "xgm/devices/Sound/nes_n106.h"
#include "xgm/devices/Sound/nes_vrc7.h"

TEST_CASE("MMC5 renders non-silent output", "[chips][mmc5]") {
    xgm::NES_MMC5 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.Write(0x5015, 0x03);   // enable both squares
    chip.Write(0x5000, 0xBF);   // sq1: 50% duty, vol 15, env off
    chip.Write(0x5002, 0xFD);   // sq1 freq low
    chip.Write(0x5003, 0x18);   // sq1 freq high + length reload
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}

TEST_CASE("FDS renders non-silent output", "[chips][fds]") {
    xgm::NES_FDS chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.SetMask(0);                          // clear mask so output is not gated
    chip.Write(0x4089, 0x80);                 // wave write mode
    for (int i = 0; i < 64; ++i) chip.Write(0x4040 + i, i >> 1);  // sawtooth
    chip.Write(0x4089, 0x00);                 // wave play, master vol max
    chip.Write(0x4080, 0xBF);                 // vol env disabled, level 63 (caps at 32)
    chip.Write(0x4084, 0x80); chip.Write(0x4087, 0x80);  // mod envelope off, mod halt
    chip.Write(0x4082, 0x40);                 // freq low = 0x40
    chip.Write(0x4083, 0x00);                 // freq high = 0, clear wav_halt
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}

TEST_CASE("N163 renders non-silent output", "[chips][n163]") {
    // N163 register map (channel 7, the sole active channel):
    //   regs 0x78-0x7F  = ch7 freq_lo, phase_lo, freq_mid, phase_mid,
    //                      freq_hi|len, phase_hi, wave_off, vol|chan_count
    // Wave data lives in any lower regs; we put a square at reg[0x00..0x07]
    // (16 nibble-packed 4-bit samples = 8 bytes: 0xFF = two samples of 15).
    xgm::NES_N106 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.SetMask(0);                          // unmask all channels

    chip.Write(0xE000, 0x00);                 // master enable (bit6=0 means on)

    // Write 8 bytes of wave data at reg[0x00..0x07] (16 nibble-packed samples = 15,15,...)
    chip.Write(0xF800, 0x80);                 // reg ptr = 0, auto-increment
    for (int i = 0; i < 8; ++i) chip.Write(0x4800, 0xFF); // 16 samples of value 15

    // Set up channel 7 registers at reg[0x78..0x7F]
    chip.Write(0xF800, 0x78);                 // reg ptr = 0x78, no auto-increment
    chip.Write(0x4800, 0x40);                 // reg[0x78]: freq bits 7:0 = 0x40
    chip.Write(0xF800, 0x7A);
    chip.Write(0x4800, 0x00);                 // reg[0x7A]: freq bits 15:8 = 0
    chip.Write(0xF800, 0x7C);
    // reg[0x7C]: freq bits 17:16 in bits 1:0; length = 256-(val&0xFC): val=0xF0 → len=16
    chip.Write(0x4800, 0xF0);
    chip.Write(0xF800, 0x7E);
    chip.Write(0x4800, 0x00);                 // reg[0x7E]: wave offset = 0
    // reg[0x7F]: bits 7:4 = channel count (0→1 channel), bits 3:0 = volume
    chip.Write(0xF800, 0x7F);
    chip.Write(0x4800, 0x0F);                 // 1 channel, volume = 15

    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}

TEST_CASE("VRC7 renders non-silent output", "[chips][vrc7]") {
    xgm::NES_VRC7 chip;
    chip.SetClock(1789772.7); chip.SetRate(48000.0); chip.Reset();
    chip.SetMask(0);
    // ch0: instrument 1 (bell-like built-in patch), volume 0 (loudest)
    chip.Write(0x9010, 0x30); chip.Write(0x9030, 0x10);  // reg $30: inst=1, vol=0
    // F-num for ~440 Hz at block 4: fnum = 440 * 2^(20-4) / (clk/36/72) ≈ 0x11B
    chip.Write(0x9010, 0x10); chip.Write(0x9030, 0x1B);  // reg $10: F-num bits 7:0
    // reg $20: bit4=key-on, bits3:1=block(4), bit0=F-num bit8(1) → 0x10|0x08|0x01 = 0x19
    // Clear key-on first (FamiTracker pattern), then set
    chip.Write(0x9010, 0x20); chip.Write(0x9030, 0x00);  // reg $20: key-off
    chip.Write(0x9010, 0x20); chip.Write(0x9030, 0x19);  // reg $20: key-on, block 4
    auto r = smokeRender(chip);
    REQUIRE(r.bounded);
    REQUIRE(r.nonSilent);
}
