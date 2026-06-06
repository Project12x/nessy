// km6502 configuration macros — must be set before the include
#define ILLEGAL_OPCODES 1
#define USE_USERPOINTER 1
#define USE_INLINEMMC   0
#define USE_CALLBACK    1
#define External static
#include "km6502m.h"
#undef ILLEGAL_OPCODES
#undef USE_USERPOINTER
#undef USE_INLINEMMC
#undef USE_CALLBACK
#undef External

#include "Km6502Harness.h"

struct Km6502Harness::Impl { K6502_Context ctx{}; };

// Callback is __fastcall on MSVC (defined in kmconfig.h).
// ReadHandler typedef: Uword (Callback *)(void *user, Uword adr)
// WriteHandler typedef: void (Callback *)(void *user, Uword adr, Uword value)
static Uword Callback harnessRead(void* user, Uword adr)
{
    return static_cast<Km6502Harness*>(user)->ram[adr & 0xFFFF];
}
static void Callback harnessWrite(void* user, Uword adr, Uword value)
{
    static_cast<Km6502Harness*>(user)->ram[adr & 0xFFFF] = static_cast<uint8_t>(value & 0xFF);
}

Km6502Harness::Km6502Harness() : impl_(std::make_unique<Impl>())
{
    impl_->ctx.ReadByte  = &harnessRead;
    impl_->ctx.WriteByte = &harnessWrite;
    impl_->ctx.user      = this;
    // K6502_INIT via iRequest: one K6502_Exec call sets A/X/Y/S/P to reset
    // values (A=0, X=0, Y=0, S=0xFF, P=Z|R|I) and clears iRequest.
    // This mirrors how nes_cpu.cpp initialises the context.
    impl_->ctx.iRequest  = K6502_INIT;
    K6502_Exec(&impl_->ctx);
}

Km6502Harness::~Km6502Harness() = default;

void Km6502Harness::load(uint16_t addr, const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        ram[(addr + i) & 0xFFFF] = data[i];
}

void Km6502Harness::setPC(uint16_t p)
{
    impl_->ctx.PC = p;
}

uint16_t Km6502Harness::pc() const
{
    return static_cast<uint16_t>(impl_->ctx.PC);
}

void Km6502Harness::step()
{
    K6502_Exec(&impl_->ctx);
}

std::optional<uint16_t> Km6502Harness::runUntilTrap(uint64_t instrLimit)
{
    for (uint64_t i = 0; i < instrLimit; ++i) {
        const uint16_t before = static_cast<uint16_t>(impl_->ctx.PC);
        K6502_Exec(&impl_->ctx);
        if (static_cast<uint16_t>(impl_->ctx.PC) == before)
            return before;
    }
    return std::nullopt;
}
