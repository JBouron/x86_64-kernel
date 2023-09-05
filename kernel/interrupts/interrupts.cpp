// Interrupts related functions and types.

#include <interrupts/interrupts.hpp>
#include <interrupts/lapic.hpp>
#include <logging/log.hpp>
#include <util/panic.hpp>
#include <util/assert.hpp>
#include "ioapic.hpp"

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
extern "C" void interruptHandler21();   extern "C" void interruptHandler32();

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
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    Descriptor(),
    // Below are IDT entries for the user-defined vector starting at vector 32.
    IDT_DESC(32),
};
// The number of elements in the IDT.
static u64 const IDT_SIZE = sizeof(IDT) / sizeof(*IDT);

// The default interrupt handler, raises a PANIC for the unhandled interrupt.
// This handler is set for all arch interrupt vectors upon Init.
void defaultHandler(Vector const vector, Frame const& frame) {
    PANIC("Got interrupt #{} from RIP = {x}", vector.raw(), frame.rip);
}

// Disable the legacy Programmable Interrupt Controller 8259.
static void disablePic() {
    Cpu::outb(0xa1, 0xff);
    Cpu::outb(0x21, 0xff);
    Log::info("PIC disabled");
}

// Array of IoApic. One such instance per I/O APIC in the system.
static IoApic ** IO_APICS = nullptr;
static u8 NUM_IO_APICS = 0;

// Create one IoApic interface for each I/O APIC present in the system.
static void initIoApics() {
    Acpi::Info const& acpiInfo(Acpi::parseTables());
    u8 const numIoApics(acpiInfo.ioApicDescSize);
    Log::info("{} I/O APIC(s) present in the system", numIoApics);
    NUM_IO_APICS = numIoApics;
    IO_APICS = new IoApic*[numIoApics];
    for (u8 i(0); i < numIoApics; ++i) {
        PhyAddr const base(acpiInfo.ioApicDesc[i].address);
        Log::info("Initializing I/O APIC with base {}", base);
        IO_APICS[i] = new IoApic(base);
    }
}

// Find the I/O APIC receiving the interrupts associated with a given GSI.
// @param gsi: The GSI for which to get the I/O APIC.
// @return: Reference to the interface of the I/O APIC handling the GSI.
static IoApic& ioApicForGsi(Acpi::Gsi const gsi) {
    Acpi::Info const& acpiInfo(Acpi::parseTables());
    for (u8 i(0); i < NUM_IO_APICS; ++i) {
        IoApic& ioApic(*IO_APICS[i]);
        // The smallest GSI handled by this IO APIC.
        Acpi::Gsi const gsiStart(acpiInfo.ioApicDesc[i].interruptBase);
        // The highest GSI handled by this IO APIC.
        Acpi::Gsi const gsiEnd(gsiStart.raw()+ioApic.numInterruptSources() - 1);
        if (gsiStart <= gsi && gsi <= gsiEnd) {
            return ioApic;
        }
    }
    PANIC("Could not find I/O APIC for GSI = {}", gsi.raw());
}

// Initialize interrupts.
void Init() {
    // Disable the legacy PIC, only use APIC.
    disablePic();

    // Enable the APIC.
    Interrupts::lapic();

    initIoApics();

    // Initialize the interrupt handlers for the non-user-defined vectors.
    for (Vector v(0); v < 32; ++v) {
        if (!v.isReserved()) {
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

// Some interrupt vectors in the x86 architecture are reserved and not used.
// Check if this vector is one of them.
// @return: true if this vector is reserved, false otherwise.
bool Vector::isReserved() const {
    return m_value == 15 || (22 <= m_value && m_value <= 31);
}

// Check if this vector is user-defined, e.g. above 32.
// @return: true if this vector is user-defined, false otherwise.
bool Vector::isUserDefined() const {
    return m_value >= 32;
}

// Get the ACPI Global System Interrupt number associated with this IRQ #.
// This requires parsing the ACPI tables to find the mapping IRQ <-> GSI.
// @return: The GSI associated with this IRQ.
Acpi::Gsi Irq::toGsi() const {
    Acpi::Info const acpiInfo(Acpi::parseTables());
    return acpiInfo.irqDesc[raw()].gsiVector;
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
    if (vector >= IDT_SIZE) {
        PANIC("IDT does not contain an entry for the target vector");
    } else if (vector.isReserved()) {
        PANIC("Cannot setup handler for a reserved vector");
    }
    INT_HANDLERS[vector.raw()] = handler;
}

// Deregister the interrupt handler that was associated with the given vector.
// If this vector refers to user-defined interrupts, any future interrupt will
// be ignored. If this is a non-user vector (e.g. < 32) any future interrupt
// will trigger a PANIC.
// @param vector: The vector for which to remove the handler.
void deregisterHandler(Vector const vector) {
    if (vector.isUserDefined()) {
        // For user-defined vector simply set the handler to nullptr. The
        // genericInterruptHandler knows to ignore such handlers.
        INT_HANDLERS[vector.raw()] = nullptr;
    } else {
        // Ignoring "system" interrupts is usually a bad idea. Hence set the
        // handler to the default handler which triggers a PANIC.
        INT_HANDLERS[vector.raw()] = defaultHandler;
    }
}

// Map an IRQ to a particular vector. This function takes care of configuring
// the I/O APIC so that a vector `vector` is raised when the given IRQ is
// asserted.
// @param irq: The IRQ to map.
// @param vector: The vector to map the IRQ to.
void mapIrq(Irq const irq, Vector const vector) {
    Acpi::Gsi const gsi(irq.toGsi());
    Log::debug("Mapping IRQ {} (GSI = {}) to Vector {}", irq.raw(), gsi.raw(),
               vector.raw());
    Acpi::Info const& acpiInfo(Acpi::parseTables());
    Acpi::Info::IrqDesc const& irqDesc(acpiInfo.irqDesc[irq.raw()]);
    IoApic& ioApic(ioApicForGsi(gsi));
    // Configure the redirection entry of the I/O APIC for the associated input
    // pin.
    // FIXME: The IoApic::Id might not correspond to the index of this IoApic in
    // the IO_APICS array!
    Acpi::Gsi const gsiBase(acpiInfo.ioApicDesc[ioApic.id()].interruptBase);
    ASSERT(gsiBase < gsi);

    IoApic::InputPin const inputPin(gsi.raw() - gsiBase.raw());
    IoApic::OutVector const outVector(vector.raw());

    // Convert the Acpi::Info::Polarity to IoApic::InputPinPolarity.
    IoApic::InputPinPolarity const polarity(
        (irqDesc.polarity == Acpi::Info::Polarity::ConformToBusSpecs
         || irqDesc.polarity == Acpi::Info::Polarity::ActiveHigh) ?
            IoApic::InputPinPolarity::ActiveHigh :
            IoApic::InputPinPolarity::ActiveLow);

    // Convert the Acpi::Info::TriggerMode to IoApic::TriggerMode.
    IoApic::TriggerMode const triggerMode(
        (irqDesc.triggerMode == Acpi::Info::TriggerMode::ConformToBusSpecs
         || irqDesc.triggerMode == Acpi::Info::TriggerMode::EdgeTriggered) ?
            IoApic::TriggerMode::Edge :
            IoApic::TriggerMode::Level);

    // FIXME: For now all interrupts are sent to CPU #0 in Fixed/Physical mode.
    IoApic::Dest const destinationApic(0x0);

    // Write the redirection entry.
    ioApic.redirectInterrupt(inputPin,
                             outVector,
                             IoApic::DeliveryMode::Fixed,
                             IoApic::DestinationMode::Physical,
                             polarity,
                             triggerMode,
                             destinationApic);
}

// Unmap an IRQ from whatever vector it was mapped to. This revert the
// operation done in map(irq, vec).
// @param irq: The IRQ to unmap.
void unmapIrq(Irq const irq) {
    Acpi::Gsi const gsi(irq.toGsi());
    Log::debug("Unmapping IRQ {} (GSI = {})", irq.raw(), gsi.raw());
    Acpi::Info const& acpiInfo(Acpi::parseTables());
    IoApic& ioApic(ioApicForGsi(gsi));
    // Configure the redirection entry of the I/O APIC for the associated input
    // pin.
    // FIXME: The IoApic::Id might not correspond to the index of this IoApic in
    // the IO_APICS array!
    Acpi::Gsi const gsiBase(acpiInfo.ioApicDesc[ioApic.id()].interruptBase);
    ASSERT(gsiBase < gsi);

    IoApic::InputPin const inputPin(gsi.raw() - gsiBase.raw());
    // Simply mask the interrupt source at the I/O APIC level.
    ioApic.setInterruptSourceMask(inputPin, true);
}

// Generic interrupt handler. _All_ interrupts are entering the C++ side of
// the kernel through this function.
// @param vector: The vector of the current interrupt.
extern "C" void genericInterruptHandler(u8 const _vector,
                                        Frame const * const frame) {
    Vector const vector(_vector);
    if (vector.isReserved()) {
        // This should never happen, unless the code jumping to this function
        // sent us a garbage vector.
        PANIC("Uh? Got an interrupt on a reserved vector. Most likely a bug");
    }
    // Check if there is an interrupt handler associated with this vector.
    InterruptHandler const& handler(INT_HANDLERS[vector.raw()]);
    if (!vector.isUserDefined() && !handler) {
        // Non-user-defined vectors always have a handler, set during Init() or
        // in deregisterHandler(). If this fails then we have a bug somewhere.
        PANIC("Non-user-defined vector #{} has no registered handler",
              vector.raw());
    }
    if (!!handler) {
        handler(vector, *frame);
    } else {
        // Ignore user-defined vectors for which there is no handler.
        Log::warn("Ignoring spurious interrupt #{} with no handler",
                  vector.raw());
    }
    lapic().endOfInterrupt();
}
}
