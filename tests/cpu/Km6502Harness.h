#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Drives the vendored km6502 core against a flat 64 KB address space.
class Km6502Harness {
public:
    Km6502Harness();
    ~Km6502Harness();

    void load(uint16_t addr, const uint8_t* data, size_t len);
    void setPC(uint16_t pc);
    uint16_t pc() const;
    void step();                              // execute one instruction
    // Run until PC stops advancing (a JMP-to-self "trap") or instrLimit is hit.
    // Returns the trap PC, or 0xFFFFFFFF if instrLimit was reached.
    uint32_t runUntilTrap(uint64_t instrLimit);

    std::array<uint8_t, 65536> ram{};

private:
    struct Impl;                              // hides km6502m.h in the .cpp
    std::unique_ptr<Impl> impl_;
};
