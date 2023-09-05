// Functions to parse the ACPI tables.
#pragma once
#include <util/subrange.hpp>

namespace Acpi {

// ACPI defines Global System Interrupts as an abstraction of interrupt vectors.
// There is a mapping between ISA IRQs and Global System Interrupts.
// Each Global System Interrupt is connected to one I/O APIC. Each I/O APIC has
// a "base" Global System Interrupt which is the GSI connected to its first
// input. An I/O APIC therefore receives interrupts from base to base +
// numInputs where numInputs is the number of inputs for this I/O APIC defined
// as the number of entries in the redirection table.
class Gsi : public SubRange<Gsi, 0, u32(~0ULL)> {};

// Holds the information of interest contained in ACPI tables.
struct Info {
    // The physical address to use to access the local APIC. The documentation
    // is not very clear if this should take priority over the IA32_APIC_BASE
    // MSR, although it seems that both methods return the same thing.
    PhyAddr localApicAddress;

    // Indicates if the system has a dual 8259 legacy PIC setup. If this is true
    // then those 8259s should be disabled before using the APICs.
    bool hasDual8259 = false;

    // Polarity of an IRQ.
    enum class Polarity : u8 {
        // The polarity is standard for this IRQ.
        ConformToBusSpecs = 0,
        ActiveHigh = 1,
        ActiveLow = 3,
    };

    // Trigger mode of an IRQ.
    enum class TriggerMode : u8 {
        // The trigger mode is standard for this IRQ.
        ConformToBusSpecs = 0,
        EdgeTriggered = 1,
        LevelTriggered = 3,
    };

    // Multiprocessor
    // --------------
    // The MADT gives information about the number of processors/cores in the
    // system and their state.
    // Information about a processor. All cores and threads have their own
    // ProcessorDesc, e.g. no distinction between physical and logical core.
    struct ProcessorDesc {
        // The ID of this processor.
        u8 id = 0;
        // ID of the LAPIC associated with this processor.
        u8 apicId = 0;
        // Indicates if this processor is enabled, e.g. it is ready for use.
        bool isEnabled = false;
        // Depends on the value of isEnabled:
        //  * isEnabled == true: This bit must be zero and is un-used.
        //  * isEnabled == false: This bit indicates if this processor can be
        //  enabled at runtime. If 0 then this processor must NOT be used.
        bool isOnlineCapable = false;
        // Indicates if this processor's LAPIC is connected to an NMI source.
        bool hasNmiSource = false;
        // The polarity and trigger mode of the NMI connected to the LAPIC. Only
        // valid if hasNmiSource == true!
        Polarity nmiPolarity = Polarity::ConformToBusSpecs;
        TriggerMode nmiTriggerMode = TriggerMode::ConformToBusSpecs;
        // Indicates to which LAPIC input pin (LIN0 or LINT1) the NMI is
        // connected to. Only valid if hasNmiSource == true!
        u8 nmiLint = 0;
    };
    // The size of the processorDesc array, incidentally the number of
    // processors in the system.
    u32 processorDescSize = 0;
    // ProcessorDesc for each processor in the system.
    ProcessorDesc* processorDesc = nullptr;

    // ISA IRQ mapping, polarity and trigger mode
    // ------------------------------------------
    // ISA IRQs 0-15 are typically identity mapped to Global System Interrupt
    // Vectors, e.g. IRQ #i is mapped to GSI vector i. However there can be
    // exceptions to this rule: for instance IRQ #0 (PIT) could be mapped to GSI
    // #2. Such exceptions are indicated in the MADT.
    // Implementation of ISA IRQs can also have non-standard polarity or trigger
    // mode, e.g. an IRQ that is usually active high edge-triggered could be
    // implemented as active low level-triggered in hardware. The I/O APIC
    // should be aware of this when configuring its input sources. The MADT also
    // indicates IRQs using non-standard polarity and/or trigger mode.
    // All information about an IRQ, e.g. its mapping to GSI, polarity and
    // trigger mode is recored in an IrqDesc. There is one IrqDesc for each IRQ#
    // (0-15).
    struct IrqDesc {
        // The Global System Interrupt vector this IRQ is mapped to. For most
        // IRQs this is an ID-map, but not necessarily!
        Gsi gsiVector;
        // The polarity of this IRQ.
        Polarity polarity = Polarity::ConformToBusSpecs;
        // The trigger mode of this IRQ.
        TriggerMode triggerMode = TriggerMode::ConformToBusSpecs;
    };
    // IrqDesc for each IRQ#.
    IrqDesc irqDesc[16];

    // I/O APICs
    // ---------
    // There can be multiple I/O APICs in a system. Each I/O APIC is responsible
    // to handle a sub-set of all Global System Interrupts. For example, in a
    // system with two I/O APICs, I/O APIC #0 could be responsible for GSI
    // vectors 0 through 24 and I/O APIC #1 could be responsible for GSI vectors
    // 25 through 48. In this situation there are two IoApicDescs, I/O APIC
    // #0's interruptBase is 0 while I/O APIC #1's base is 24. In other words,
    // I/O APIC #0's input 0 is wired to GSI 0, input 1 is wired to GSI 1, ...
    // while I/O APIC #1's input 0 is wired to GSI 24, input 1 is wired to GSI
    // 25, ...
    // Details about an I/O APIC in the system.
    struct IoApicDesc {
        // The ID of this I/O APIC.
        u8 id = 0;
        // The physical address this I/O APIC is mapped to.
        PhyAddr address;
        // The base GSI of this I/O APIC. This is the GSI vector mapped to the
        // first input pin of this I/O APIC. This I/O APIC therefore handles GSI
        // vectors interruptBase through interruptBase+size where size is the
        // number of input pins on this I/O APIC.
        Gsi interruptBase;
    };
    // The size of the ioApicDesc array.
    u8 ioApicDescSize = 0;
    // One IoApicDesc per I/O APIC present in the system.
    IoApicDesc* ioApicDesc = nullptr;

    // Non-Maskable-Interrupt
    // ----------------------
    // Disclaimer: The ACPI specification is not clear if there can be multiple
    // NMI sources, that is multiple "NMI source" entries in the MADT. I assume
    // that this is possible.
    // The MADT indicates which GSI(s) correspond to an NMI. I/O APIC with an
    // NMI GSI input should be configured appropriately.
    // Details about an NMI source.
    struct NmiSourceDesc {
        // The polarity of this NMI source.
        Polarity polarity = Polarity::ConformToBusSpecs;
        // The trigger mode of this NMI source.
        TriggerMode triggerMode = TriggerMode::ConformToBusSpecs;
        // The GSI vector to which this NMI source is mapped.
        Gsi gsiVector;
    };
    // The size of the nmiSourceDesc array.
    u8 nmiSourceDescSize = 0;
    // One NmiSourceDesc per NMI source.
    NmiSourceDesc* nmiSourceDesc = nullptr;
};

// Parse the ACPI tables found in BIOS memory.
// @return: Reference to an Info struct containing the info parsed from the ACPI
// tables. Calling this function multiple times always return the same
// reference.
Info const& parseTables();
}
