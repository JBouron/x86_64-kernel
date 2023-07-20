// Interrupts related functions and types.

#include <interrupts/interrupts.hpp>
#include <logging/log.hpp>
#include <util/panic.hpp>

namespace Interrupts {

// Construct a default descriptor, marked as non-present.
Descriptor::Descriptor() : m_raw{0, 0, 0, 0} {}

// Construct a descriptor. The descriptor is marked as present.
// @param targetSel: The target code segment selector.
// @param targetOffset: Virtual address of the interrupt handler.
// @param dpl: Privilege level of the descriptor.
// @param type: The type of this descriptor.
Descriptor::Descriptor(Cpu::SegmentSel const targetSel,
                       u64 const targetOffset,
                       Cpu::PrivLevel const dpl,
                       Type const type) :
    m_raw{
        (u32(targetSel.raw()) << 16) | u32(targetOffset & 0xffff),
        u32(targetOffset & 0xffff0000)|(1<<15)|(u32(dpl)<<13)|(u32(type)<<8),
        u32(targetOffset >> 32),
        0,
    } {}

// Interrupt handlers for each vector are defined interruptsAsm.asm. Each
// eventually call genericInterruptHandler defined further down this file.
extern "C" void interruptHandler0();    extern "C" void interruptHandler1();
extern "C" void interruptHandler2();    extern "C" void interruptHandler3();
extern "C" void interruptHandler4();    extern "C" void interruptHandler5();
extern "C" void interruptHandler6();    extern "C" void interruptHandler7();
extern "C" void interruptHandler8();    extern "C" void interruptHandler10();
extern "C" void interruptHandler11();   extern "C" void interruptHandler12();
extern "C" void interruptHandler13();   extern "C" void interruptHandler14();
extern "C" void interruptHandler16();   extern "C" void interruptHandler17();
extern "C" void interruptHandler18();   extern "C" void interruptHandler19();
extern "C" void interruptHandler21();

// Create a Descriptor for the given vector. The descriptor points to the
// interruptHandler<vector> function.
// FIXME: Don't hardcode the code seg selector here.
#define IDT_DESC(vector) \
    Descriptor(Cpu::SegmentSel(1, Cpu::PrivLevel::Ring0), \
               reinterpret_cast<u64>(interruptHandler ## vector), \
               Cpu::PrivLevel::Ring0, \
               Descriptor::Type::InterruptGate)

// The kernel-wide IDT.
static const Descriptor IDT[] = {
    IDT_DESC(0),
    IDT_DESC(1),
    IDT_DESC(2),
    IDT_DESC(3),
    IDT_DESC(4),
    IDT_DESC(5),
    IDT_DESC(6),
    IDT_DESC(7),
    IDT_DESC(8),
    Descriptor(),
    IDT_DESC(10),
    IDT_DESC(11),
    IDT_DESC(12),
    IDT_DESC(13),
    IDT_DESC(14),
    Descriptor(),
    IDT_DESC(16),
    IDT_DESC(17),
    IDT_DESC(18),
    IDT_DESC(19),
    Descriptor(),
    IDT_DESC(21),
};

// Generic interrupt handler. _All_ interrupts are entering the C++ side of
// the kernel through this function.
// @param vector: The vector of the current interrupt.
extern "C" void genericInterruptHandler(u8 const vector) {
    PANIC("Got interrupt #{}", vector);
}

// Initialize interrupts.
void Init() {
    u64 const base(reinterpret_cast<u64>(IDT));
    u16 const limit(sizeof(IDT) - 1);
    Cpu::TableDesc const idtDesc(base, limit);
    Log::info("Loading IDT, base = {x}, limit = {}", base, limit);
    Cpu::lidt(idtDesc);
    Log::debug("IDT loaded");
}
}
