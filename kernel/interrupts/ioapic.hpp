// Functions related to I/O APIC.
#pragma once
#include <util/addr.hpp>
#include <acpi/acpi.hpp>
#include <selftests/selftests.hpp>

namespace Interrupts {

// Interface to interact with an I/O APIC. There is instance of this class per
// I/O APIC in the system.
class IoApic {
public:
    // Create an interface for an I/O APIC located at the given physical
    // address.
    // @param base: The base physical addres of this I/O APIC.
    IoApic(PhyAddr const base);

    virtual ~IoApic() = default;

    // ID type used by I/O APICs.
    using Id = u8;

    // Get the ID of this I/O APIC.
    Id id() const;

    // Version type used by I/O APICs.
    using Version = u8;

    // Get the version of this I/O APIC.
    Version version() const;

    // Get the arbitration ID of this I/O APIC.
    Id arbitrationId() const;

    // Get the number of input interrupts handled by this I/O APIC.
    u8 numInterruptSources() const;

    // Interrupt redirection and routing
    // ---------------------------------
    // The main goal of the I/O APIC is to map interrupt sources (e.g. IRQ#) to
    // particular interrupt vectors and send vectors to one or more local APICs.
    // The following types and functions provide a way to configure input
    // interrupts of the I/O APIC.

    // Represent an input pin of an I/O APIC. Input pins are identified by an
    // index from 0 to the number of redirection entries (e.g. the number of
    // interrupt pins). I/O APIC specification indicates that there might be a
    // maximum of 239 input pin in an I/O APIC.
    class InputPin : public SubRange<InputPin, 0, 239> {};

    // An I/O APIC can only map interrupts to vectors in 0x10 - 0xfe. This type
    // makes sure that no one is trying to re-map to an illegal interrupt
    // vector.
    class OutVector : public SubRange<OutVector, 0x10, 0xfe> {};

    // Delivery mode for a vector forwarded to one or more local APICs.
    enum class DeliveryMode : u8 {
        // Deliver the signal on the INTR signal of all processor cores listed
        // in the destination. Trigger Mode for "fixed" Delivery Mode can be
        // edge or level.
        Fixed = 0b000,
        // Deliver the signal on the INTR signal of the processor core that is
        // executing at the lowest priority among all the processors listed in
        // the specified destination. Trigger Mode for "lowest priority"
        // Delivery Mode can be edge or level.
        LowestPriority = 0b001,
        // System Management Interrupt. A delivery mode equal to SMI requires an
        // edge trigger mode. The vector information is ignored but must be
        // programmed to all zeroes for future compatibility.
        Smi = 0b010,
        // Deliver the signal on the NMI signal of all processor cores listed in
        // the destination. Vector information is ignored. NMI is treated as an
        // edge triggered interrupt, even if it is programmed as a level
        // triggered interrupt. For proper operation, this redirection table
        // entry must be programmed to “edge” triggered interrupt.
        Nmi = 0b100,
        // Deliver the signal to all processor cores listed in the destination
        // by asserting the INIT signal. All addressed local APICs will assume
        // their INIT state. INIT is always treated as an edge triggered
        // interrupt, even if programmed otherwise. For proper operation, this
        // redirection table entry must be programmed to “edge” triggered
        // interrupt.
        Init = 0b101,
        // Deliver the signal to the INTR signal of all processor cores listed
        // in the destination as an interrupt that originated in an externally
        // connected (8259A-compatible) interrupt controller. The INTA cycle
        // that corresponds to this ExtINT delivery is routed to the external
        // controller that is expected to supply the vector. A Delivery Mode of
        // "ExtINT" requires an edge trigger mode.
        ExtInt = 0b111,
    };

    // Destination mode for a vector to be fowarded to one or more local APICs.
    // This determines the interpretation of the Destination.
    enum class DestinationMode : u8 {
        // Bits 56 through 59 of the Destination field specify the 4 bit APIC ID
        // to send the vector to. Note that this means only cpus with IDs <= 16
        // can receive an interrupt from the I/O APIC in Physical mode.
        Physical = 0,
        // The destination local APIC(s) are identified by matching on the
        // logical destination under the control of the Destination Format
        // Register and Logical Destination Register in each Local APIC.
        // Multiple local APICs may use the same LDR, in which case they may all
        // receive the vector, as long as the delivery mode is not
        // LowestPriority.
        Logical = 1,
    };

    // Polarity of the input interrupt pin.
    enum class InputPinPolarity : u8 {
        ActiveHigh = 0,
        ActiveLow = 1,
    };

    // Trigger mode of the input interrupt pin and mapped vector.
    enum class TriggerMode : u8 {
        Edge = 0,
        Level = 1,
    };

    // The destination of an input interrupt redirection. Its interpretation
    // depends on the value of the DestinationMode.
    using Dest = u8;

    // Redirect a particular source interrupt to a given vector and configure
    // how this interrupt should be routed. The input pin is unmasked as a
    // result.
    // @param inputPin: The input pin to be redirected.
    // @param outVector: The vector that should be mapped to the input pin.
    // @param deliveryMode: Configures how the vector should be delivered to its
    // destination.
    // @param destinationMode: Indicates how the destination for this vector is
    // interpreted from the `destinationApic` value.
    // @param polarity: The polarity of the input pin.
    // @param triggerMode: The trigger mode of the input pin. This also
    // indicates the trigger mode of the mapped vector.
    // @param destinationApic: The destination of the vector. Its interpretation
    // depends on the value of the destinationMode.
    void redirectInterrupt(InputPin const inputPin,
                           OutVector const outVector,
                           DeliveryMode const deliveryMode,
                           DestinationMode const destinationMode,
                           InputPinPolarity const polarity,
                           TriggerMode const triggerMode,
                           Dest const destinationApic);

    // (Un-)Mask an input pin of this I/O APIC. Only the `mask` bit is written
    // in the redirection entry, the other bits are untouched so that the
    // interrupt may be un-masked again.
    // @param inputPin: The input pin to mask.
    // @param isMasked: If true, masks the given interrupt pin, otherwise unmask
    // it.
    void setInterruptSourceMask(InputPin const inputPin, bool const isMasked);

    // Testing related functions.
    // Run the I/O APIC tests.
    static void Test(SelfTests::TestRunner& runner);
    friend SelfTests::TestResult ioApicRedirectionTableEntryTest();
    friend SelfTests::TestResult ioApicReadRegisterTest();
    friend SelfTests::TestResult ioApicReadWriteRedirectionTableTest();
    friend SelfTests::TestResult
        ioApicWriteRedirectionTableEntryReservedBitTest();
    friend SelfTests::TestResult ioApicMaskInterruptSourceTest();
    friend SelfTests::TestResult ioApicRedirectInterruptTest();

protected:
    // The base physical address of this I/O APIC.
    PhyAddr m_base;

    // The I/O APIC register indices.
    enum class Register : u8 {
        // ID.
        IOAPICID = 0x00,
        // Version.
        IOAPICVER = 0x01,
        // Arbitration ID.
        IOAPICARB = 0x02,
        // Base index of the Redirection Table entries.
        IOREDTBL_BASE = 0x10,
    };

    // Read an I/O APIC register.
    // @param src: The register to read.
    // @return: The current value of the register `src`.
    virtual u32 readRegister(Register const src) const;

    // Write into an I/O APIC register. Note: This function does NOT skip
    // reserved bits in registers, it is the responsibility of the caller to
    // make sure not to change the value of a reserved bit.
    // @param dest: The register to write into.
    // @param value: The value to write into the destination register.
    virtual void writeRegister(Register const dest, u32 const value);

    // Type for an entry in the redirection table.
    class RedirectionTableEntry {
    public:
        // Create a default entry that is masked.
        RedirectionTableEntry();

        // Create a RedirectionTableEntry from a raw u64, e.g. from an actual
        // entry stored in a register.
        // @param raw: The raw value to create the RedirectionTableEntry from.
        RedirectionTableEntry(u64 const raw);

        // Create a RedirectionTableEntry with the given config.
        // @param outVector: The output vector for this entry.
        // @param deliveryMode: The delivery mode for the output vector.
        // @param destinationMode: The destination mode for the output vector.
        // @param polarity: The polarity of the input pin associated with this
        // entry.
        // @param triggerMode: The trigger mode of the input pin and output
        // vector.
        // @param destinationApic: The destination of the output vector.
        RedirectionTableEntry(Vector const outVector,
                              DeliveryMode const deliveryMode,
                              DestinationMode const destinationMode,
                              InputPinPolarity const polarity,
                              TriggerMode const triggerMode,
                              Dest const destinationApic);

        // Set the `mask` bit of this RedirectionTableEntry, the rest of the
        // bits are unchanged. Useful when toggling the masking of an existing
        // entry.
        // @param isMasked: Controls the new value of the mask bit.
        void setMasked(bool const isMasked);

        // Get the raw u64 value of this RedirectionTableEntry. Used when
        // actually writing the value into a register.
        u64 raw() const;
    private:
        Vector m_outVector;
        DeliveryMode m_deliveryMode;
        DestinationMode m_destinationMode;
        InputPinPolarity m_polarity;
        TriggerMode m_triggerMode;
        bool m_masked;
        Dest m_destinationApic;
    };

    // Compute the Register value to use to access the low and DWORD of an entry
    // in the redirection table.
    // @param entryIndex: The index of the entry.
    // @return: The Register for the low DWORD of the entry.
    static Register redirectionTableEntryRegLow(u8 const entryIndex);

    // Compute the Register value to use to access the high and DWORD of an
    // entry in the redirection table.
    // @param entryIndex: The index of the entry.
    // @return: The Register for the high DWORD of the entry.
    static Register redirectionTableEntryRegHigh(u8 const entryIndex);

    // Mask for the reserved bits in a Redirection Table entry.
    static u64 const RedirectionTableEntryReservedBits = 0x00fffffffffe5000ULL;

    // Read an entry from the redirection table.
    // @param entryIndex: The index of the entry to read.
    // @return: A RedirectionTableEntry describing the current entry's
    // configuration.
    RedirectionTableEntry readRedirectionTable(u8 const entryIndex) const;

    // Write an entry into the redirection table.
    // @param entryIndex: The index of the entry to write.
    // @param value: The value the entry should be set to.
    void writeRedirectionTable(u8 const entryIndex,
                               RedirectionTableEntry const entry);

    // The IOREGSEL register used to access the I/O APIC's registers.
    Register volatile * m_ioRegSel;
    // The IOWIN register used to read and write from the I/O APIC register
    // indicated in the IOREGSEL.
    u32 volatile * m_ioWin;
};
}
