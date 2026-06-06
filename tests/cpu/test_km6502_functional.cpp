#include <catch2/catch_test_macros.hpp>
#include "Km6502Harness.h"

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

    const uint32_t trap = cpu.runUntilTrap(100);
    REQUIRE(trap == 0x0604);          // JMP-to-itself trap
    REQUIRE(cpu.ram[0x10] == 0x42);   // STA wrote the accumulator
}
