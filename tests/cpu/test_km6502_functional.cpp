#include <catch2/catch_test_macros.hpp>
#include "Km6502Harness.h"
#include <fstream>
#include <vector>

TEST_CASE("test harness builds and runs", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("km6502 executes a basic program", "[cpu]") {
    Km6502Harness cpu;
    // $0600: LDA #$42  (A9 42)
    // $0602: STA $10   (85 10)
    // $0604: JMP $0604 (4C 04 06)  -- self-loop trap
    const uint8_t prog[] = { 0xA9, 0x42, 0x85, 0x10, 0x4C, 0x04, 0x06 };
    cpu.load(0x0600, prog, sizeof(prog));
    cpu.setPC(0x0600);

    const auto trap = cpu.runUntilTrap(100);
    REQUIRE(trap.has_value());
    REQUIRE(*trap == 0x0604);          // JMP-to-itself trap
    REQUIRE(cpu.ram[0x10] == 0x42);    // STA wrote the accumulator
}

TEST_CASE("km6502 passes the Klaus Dormann functional test", "[cpu][functional]") {
    std::ifstream f("roms/6502_functional_test.bin", std::ios::binary);
    REQUIRE(f.good());                                  // ROM present next to the exe
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE(rom.size() == 65536);

    Km6502Harness cpu;
    cpu.load(0x0000, rom.data(), rom.size());
    cpu.setPC(0x0400);                                  // test entry point

    // ~96M cycles (~tens of millions of instructions); cap generously, then assert success.
    const auto trap = cpu.runUntilTrap(300'000'000ull);
    REQUIRE(trap.has_value());                          // it must trap, not run forever
    INFO("trapped at PC = 0x" << std::hex << (trap ? *trap : 0));
    REQUIRE(*trap == 0x3469);                           // success self-loop
}
