// Interrupts related functions and types.

#pragma once
#include <cpu/cpu.hpp>
#include <selftests/selftests.hpp>

namespace Interrupts {

// Element of the IDT. Describes an interrupt handling routine.
class Descriptor {
public:
    // Type of the IDT descriptor.
    enum class Type {
        // Masks interrupts upon entering an interrupt handler.
        InterruptGate = 0xe,
        // Does not mask interrupts upon entering an interrupt handler.
        TrapGate = 0xf,
    };

    // Construct a descriptor. The descriptor is marked as present.
    // @param targetSel: The target code segment selector.
    // @param targetOffset: Virtual address of the interrupt handler.
    // @param dpl: Privilege level of the descriptor.
    // @param type: The type of this descriptor.
    Descriptor(Cpu::SegmentSel const targetSel,
               u64 const targetOffset,
               Cpu::PrivLevel const dpl,
               Type const type);

    // Construct a default descriptor, marked as non-present.
    Descriptor();

private:
    // The four raw DWORDs making up the descriptor, as expected by the
    // hardware.
    u32 const m_raw[4];
};
static_assert(sizeof(Descriptor) == 16);

// Initialize interrupts.
void Init();

// Run the interrupt tests.
void Test(SelfTests::TestRunner& runner);

}
