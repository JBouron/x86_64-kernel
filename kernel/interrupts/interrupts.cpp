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

// The default interrupt handler, raises a PANIC for the unhandled interrupt.
// This handler is set for all arch interrupt vectors upon Init.
void defaultHandler(Vector const vector) {
    PANIC("Got interrupt #{}", vector);
}

// Check if a particular interrupt vector is reserved.
// FIXME: This should be a member function in the Vector type.
// @param vector: The vector to test.
// @return: true if the vector is reserved per x86's definition, false
// otherwise.
static bool isVectorReserved(Vector const vector) {
    return vector == 15 || (22 <= vector && vector <= 31);
}

// Check if a particular interrupt vector is user-defined.
// FIXME: This should be a member function in the Vector type.
// @param vector: The vector to test.
// @return: true if the vector is user-defined per x86's definition, false
// otherwise.
static bool isVectorUserDefined(Vector const vector) {
    return vector >= 32;
}

// Disable the legacy Programmable Interrupt Controller 8259.
static void disablePic() {
    Cpu::outb(0xa1, 0xff);
    Cpu::outb(0x21, 0xff);
    Log::info("PIC disabled");
}

// Initialize interrupts.
void Init() {
    // Disable the legacy PIC, only use APIC.
    disablePic();

    // Initialize the interrupt handlers for the non-user-defined vectors.
    for (Vector v(0); v < 32; ++v) {
        if (!isVectorReserved(v)) {
            registerHandler(v, defaultHandler);
        }
    }

    u64 const base(reinterpret_cast<u64>(IDT));
    u16 const limit(sizeof(IDT) - 1);
    Cpu::TableDesc const idtDesc(base, limit);
    Log::info("Loading IDT, base = {x}, limit = {}", base, limit);
    Cpu::lidt(idtDesc);
    Log::debug("IDT loaded");
}

// The mapping vector -> InterruptHandler. A nullptr in this collection
// indicates that no handler is registered for a particular vector, in which
// case interrupts of that vector are ignored.
// FIXME: For now there can only be a single InterruptHandler per vector.
// Eventually, we may want to be able to have more.
static InterruptHandler INT_HANDLERS[256] = {nullptr};

// Register an interrupt handler for a particular vector. After this call, any
// interrupt with vector `vector` triggers a call to `handler`. It is an error
// to attempt to add a handler for a vector that is reserved per the x86
// architecture.
// @param vector: The vector for which to register the handler.
// @param handler: The function to be called everytime an interrupt with vector
// `vector` is raised.
void registerHandler(Vector const vector, InterruptHandler const& handler) {
    if (isVectorReserved(vector)) {
        PANIC("Cannot setup handler for a reserved vector");
    }
    INT_HANDLERS[vector] = handler;
}

// Deregister the interrupt handler that was associated with the given vector.
// If this vector refers to user-defined interrupts, any future interrupt will
// be ignored. If this is a non-user vector (e.g. < 32) any future interrupt
// will trigger a PANIC.
// @param vector: The vector for which to remove the handler.
void deregisterHandler(Vector const vector) {
    if (isVectorUserDefined(vector)) {
        // For user-defined vector simply set the handler to nullptr. The
        // genericInterruptHandler knows to ignore such handlers.
        INT_HANDLERS[vector] = nullptr;
    } else {
        // Ignoring "system" interrupts is usually a bad idea. Hence set the
        // handler to the default handler which triggers a PANIC.
        INT_HANDLERS[vector] = defaultHandler;
    }
}

// Generic interrupt handler. _All_ interrupts are entering the C++ side of
// the kernel through this function.
// @param vector: The vector of the current interrupt.
extern "C" void genericInterruptHandler(u8 const vector) {
    if (isVectorReserved(vector)) {
        // This should never happen, unless the code jumping to this function
        // sent us a garbage vector.
        PANIC("Uh? Got an interrupt on a reserved vector. Most likely a bug");
    }
    // Check if there is an interrupt handler associated with this vector.
    InterruptHandler const& handler(INT_HANDLERS[vector]);
    if (!isVectorUserDefined(vector) && !handler) {
        // Non-user-defined vectors always have a handler, set during Init() or
        // in deregisterHandler(). If this fails then we have a bug somewhere.
        PANIC("Non-user-defined vector #{} has no registered handler", vector);
    }
    if (!!handler) {
        handler(vector);
    } else {
        // Ignore user-defined vectors for which there is no handler.
        Log::warn("Ignoring spurious interrupt #{} with no handler", vector);
    }
}
}
