// Interrupts related functions and types.

#pragma once
#include <cpu/cpu.hpp>
#include <selftests/selftests.hpp>
#include <util/subrange.hpp>
#include <acpi/acpi.hpp>

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

// Only 256 possible interrupt vectors per x86's architecture.
class Vector : public SubRange<Vector, 0, 255> {
public:
    // Some interrupt vectors in the x86 architecture are reserved and not used.
    // Check if this vector is one of them.
    // @return: true if this vector is reserved, false otherwise.
    bool isReserved() const;

    // Check if this vector is user-defined, e.g. above 32.
    // @return: true if this vector is user-defined, false otherwise.
    bool isUserDefined() const;
};

// Hardware interrupt number.
class Irq : public SubRange<Irq, 0, 15> {
public:
    // Get the ACPI Global System Interrupt number associated with this IRQ #.
    // This requires parsing the ACPI tables to find the mapping IRQ <-> GSI.
    // @return: The GSI associated with this IRQ.
    Acpi::Gsi toGsi() const;
};

// An interrupt frame contains the values of all the registers of the
// interrupted context + some information about the interrupt itself, e.g. error
// code.
struct Frame {
    // The error code associated with the interrupt. If the vector of the
    // interrupt does not generate an error code this field is set to 0.
    u64 const errorCode;

    // The register values of the interrupted context. For now we only include
    // general purpose registers and not the segment registers. Those will only
    // become useful once we have a userspace.
    u64 const rip;
    u64 const rflags;
    u64 const rsp;
    u64 const rbp;
    u64 const r15;
    u64 const r14;
    u64 const r13;
    u64 const r12;
    u64 const r11;
    u64 const r10;
    u64 const r9;
    u64 const r8;
    u64 const rsi;
    u64 const rdi;
    u64 const rdx;
    u64 const rcx;
    u64 const rbx;
    u64 const rax;
} __attribute__((packed));

// Interrupt handlers are functions taking the vector and the Frame as argument
// and returning void. Passing the vector as argument is useful if a single
// handler is expected to handle multiple vectors.
using InterruptHandler = void (*)(Vector const, Frame const& frame);

// Register an interrupt handler for a particular vector. After this call, any
// interrupt with vector `vector` triggers a call to `handler`. It is an error
// to attempt to add a handler for a vector that is reserved per the x86
// architecture.
// @param vector: The vector for which to register the handler.
// @param handler: The function to be called everytime an interrupt with vector
// `vector` is raised.
void registerHandler(Vector const vector, InterruptHandler const& handler);

// Deregister the interrupt handler that was associated with the given vector.
// If this vector refers to user-defined interrupts, any future interrupt will
// be ignored. If this is a non-user vector (e.g. < 32) any future interrupt
// will trigger a PANIC.
// @param vector: The vector for which to remove the handler.
void deregisterHandler(Vector const vector);

// Map an IRQ to a particular vector. This function takes care of configuring
// the I/O APIC so that a vector `vector` is raised when the given IRQ is
// asserted.
// @param irq: The IRQ to map.
// @param vector: The vector to map the IRQ to.
void mapIrq(Irq const irq, Vector const vector);

// Unmap an IRQ from whatever vector it was mapped to. This revert the
// operation done in map(irq, vec).
// @param irq: The IRQ to unmap.
void unmapIrq(Irq const irq);

// Mask a particular IRQ. This function configures the I/O APIC to ignore
// subsequent interrupts for this IRQ.
// @param irq: The IRQ to mask.
void maskIrq(Irq const irq);

// Run the interrupt tests.
void Test(SelfTests::TestRunner& runner);

}
