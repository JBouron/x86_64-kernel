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
        (u64(targetSel.raw()) << 16) | (targetOffset & 0xffff),
        (targetOffset & 0xffff0000) | (1<<15) | (u64(dpl)<<13) | (u64(type)<<8),
        targetOffset >> 32,
        0,
    } {}

// The kernel-wide IDT. Empty for now.
static const Descriptor IDT[] = { Descriptor() };

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
