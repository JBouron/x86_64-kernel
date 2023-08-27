// Functions related to I/O APIC.
#include "ioapic.hpp"
#include <util/assert.hpp>
#include <paging/paging.hpp>

namespace Interrupts {

// Create an interface for an I/O APIC located at the given physical address.
// @param base: The base physical addres of this I/O APIC.
IoApic::IoApic(PhyAddr const base) : m_base(base) {
    // Change the mapping in the page table to be uncachable and writethrough.
    ASSERT(base.isPageAligned());
    VirAddr const vaddr(Paging::toVirAddr(base));
    Paging::PageAttr const attr(Paging::PageAttr::Writable
                                | Paging::PageAttr::WriteThrough
                                | Paging::PageAttr::CacheDisable);
    Err const err(Paging::map(vaddr, base, attr, 1));
    if (err) {
        PANIC("Failed to map I/O APIC @{}: {}", base, err.error());
    }

    m_ioRegSel = vaddr.ptr<Register volatile>();
    m_ioWin = (vaddr + 0x10).ptr<u32 volatile>();

    Log::info("Initialized I/O APIC @{}", base);
    Log::info("  ID                      = {}", id());
    Log::info("  Version                 = {x}", version());
    Log::info("  Arbitration ID          = {}", arbitrationId());
    Log::info("  Num redirection entries = {}", numInterruptSources());
}

// Get the ID of this I/O APIC.
IoApic::Id IoApic::id() const {
    // The ID is contained in bits 27:24. The rest is reserved.
    return (readRegister(Register::IOAPICID) >> 24) & 0xf;
}

// Get the version of this I/O APIC.
IoApic::Version IoApic::version() const {
    return readRegister(Register::IOAPICVER) & 0xff;
}

// Get the arbitration ID of this I/O APIC.
IoApic::Id IoApic::arbitrationId() const {
    // The ID is contained in bits 27:24. The rest is reserved.
    return (readRegister(Register::IOAPICARB) >> 24) & 0xf;
}

// Get the number of input interrupts handled by this I/O APIC.
u8 IoApic::numInterruptSources() const {
    // This number is essentially the number of redirection entries.
    return (readRegister(Register::IOAPICVER) >> 16) & 0xff;
}

// Redirect a particular source interrupt to a given vector and configure how
// this interrupt should be routed. The input pin is unmasked as a result.
// @param inputPin: The input pin to be redirected.
// @param outVector: The vector that should be mapped to the input pin.
// @param deliveryMode: Configures how the vector should be delivered to its
// destination.
// @param destinationMode: Indicates how the destination for this vector is
// interpreted from the `destinationApic` value.
// @param polarity: The polarity of the input pin.
// @param triggerMode: The trigger mode of the input pin.
// @param destinationApic: The destination of the vector. Its interpretation
// depends on the value of the destinationMode.
void IoApic::redirectInterrupt(InputPin const inputPin,
                               OutVector const outVector,
                               DeliveryMode const deliveryMode,
                               DestinationMode const destinationMode,
                               InputPinPolarity const polarity,
                               TriggerMode const triggerMode,
                               Dest const destinationApic) {
    // Validate the configuration per the I/O APIC specification.
    if (deliveryMode == DeliveryMode::Smi) {
        if (triggerMode != TriggerMode::Edge) {
            PANIC("SMI delivery mode requires Edge trigger mode");
        } else if (outVector != 0) {
            // FIXME: OutVector is working against us here. There is no way to
            // set it to 0 from the caller.
            PANIC("SMI delivery mode must specify a vector of 0");
        }
    } else if (deliveryMode == DeliveryMode::Nmi
               && triggerMode != TriggerMode::Edge) {
            PANIC("NMI delivery mode requires Edge trigger mode");
    } else if (deliveryMode == DeliveryMode::Init
               && triggerMode != TriggerMode::Edge) {
            PANIC("INIT delivery mode requires Edge trigger mode");
    } else if (deliveryMode == DeliveryMode::ExtInt
               && triggerMode != TriggerMode::Edge) {
            PANIC("ExtINT delivery mode requires Edge trigger mode");
    }

    // Configuration is validate, now write the entry associated with the input
    // pin.
    if (inputPin >= numInterruptSources()) {
        PANIC("Input pin number {} is out of bounds, must be < {}",
              inputPin.raw(),
              numInterruptSources());
    }
    RedirectionTableEntry const redirEntry(Vector(outVector.raw()),
        deliveryMode, destinationMode, polarity, triggerMode, destinationApic);
    writeRedirectionTable(inputPin.raw(), redirEntry);
}

// Mask an input pin of this I/O APIC.
// @param inputPin: The input pin to mask.
void IoApic::maskInterruptSource(InputPin const inputPin) {
    // FIXME: Bound check.
    RedirectionTableEntry entry(readRedirectionTable(inputPin.raw()));
    entry.setMasked(true);
    writeRedirectionTable(inputPin.raw(), entry);
}

// Read an I/O APIC register.
// @param src: The register to read.
// @return: The current value of the register `src`.
u32 IoApic::readRegister(Register const src) const {
    *m_ioRegSel = src;
    return *m_ioWin;
}

// Write into an I/O APIC register. Note: This function does NOT skip reserved
// bits in registers, it is the responsibility of the caller to make sure not to
// change the value of a reserved bit.
// @param dest: The register to write into.
// @param value: The value to write into the destination register.
void IoApic::writeRegister(Register const dest, u32 const value) {
    *m_ioRegSel = dest;
    *m_ioWin = value;
}

// Create a default entry that is masked.
IoApic::RedirectionTableEntry::RedirectionTableEntry() :
    m_outVector      {},
    m_deliveryMode   {},
    m_destinationMode{},
    m_polarity       {},
    m_triggerMode    {},
    m_masked         (true),
    m_destinationApic{} {}

// Create a RedirectionTableEntry from a raw u64, e.g. from an actual entry
// stored in a register.
// @param raw: The raw value to create the RedirectionTableEntry from.
IoApic::RedirectionTableEntry::RedirectionTableEntry(u64 const raw) :
    m_outVector      (raw & 0xff),
    m_deliveryMode   (static_cast<DeliveryMode>((raw >> 8) & 0x7)),
    m_destinationMode(static_cast<DestinationMode>((raw >> 11) & 0x1)),
    m_polarity       (static_cast<InputPinPolarity>((raw >> 13) & 0x1)),
    m_triggerMode    (static_cast<TriggerMode>((raw >> 15) & 0x1)),
    m_masked         (!!(raw & (1 << 16))),
    m_destinationApic(raw >> 56) {}

// Create a RedirectionTableEntry with the given config.
// @param outVector: The output vector for this entry.
// @param deliveryMode: The delivery mode for the output vector.
// @param destinationMode: The destination mode for the output vector.
// @param polarity: The polarity of the input pin associated with this entry.
// @param triggerMode: The trigger mode of the input pin and output vector.
// @param destinationApic: The destination of the output vector.
IoApic::RedirectionTableEntry::RedirectionTableEntry(
    Vector const outVector,
    DeliveryMode const deliveryMode,
    DestinationMode const destinationMode,
    InputPinPolarity const polarity,
    TriggerMode const triggerMode,
    Dest const destinationApic) :
    m_outVector      (outVector),
    m_deliveryMode   (deliveryMode),
    m_destinationMode(destinationMode),
    m_polarity       (polarity),
    m_triggerMode    (triggerMode),
    m_masked         (false),
    m_destinationApic(destinationApic) {}

// Set the `mask` bit of this RedirectionTableEntry, the rest of the bits are
// unchanged. Useful when toggling the masking of an existing entry.
// @param isMasked: Controls the new value of the mask bit.
void IoApic::RedirectionTableEntry::setMasked(bool const isMasked) {
    m_masked = isMasked;
}

// Get the raw u64 value of this RedirectionTableEntry. Used when actually
// writing the value into a register.
u64 IoApic::RedirectionTableEntry::raw() const {
    return (u64(static_cast<u8>(m_destinationApic)) << 56)
        | (u64(m_masked) << 16)
        | (u64(static_cast<u8>(m_triggerMode)) << 15)
        | (u64(static_cast<u8>(m_polarity)) << 13)
        | (u64(static_cast<u8>(m_destinationMode)) << 11)
        | (u64(static_cast<u8>(m_deliveryMode)) << 8)
        | m_outVector.raw();
}

// Compute the Register value to use to access the low and DWORD of an entry
// in the redirection table.
// @param entryIndex: The index of the entry.
// @return: The Register for the low DWORD of the entry.
IoApic::Register IoApic::redirectionTableEntryRegLow(u8 const entryIndex) {
    // 2 u32 registers per entry hence the index must be scaled by 2.
    return static_cast<Register>(static_cast<u8>(Register::IOREDTBL_BASE)
                                 + entryIndex * 2);
}

// Compute the Register value to use to access the high and DWORD of an
// entry in the redirection table.
// @param entryIndex: The index of the entry.
// @return: The Register for the high DWORD of the entry.
IoApic::Register IoApic::redirectionTableEntryRegHigh(u8 const entryIndex) {
    Register const lowReg(redirectionTableEntryRegLow(entryIndex));
    return static_cast<Register>(static_cast<u8>(lowReg) + 1);
}

// Read an entry from the redirection table.
// @param entryIndex: The index of the entry to read.
// @return: A RedirectionTableEntry describing the current entry's
// configuration.
IoApic::RedirectionTableEntry IoApic::readRedirectionTable(
    u8 const entryIndex) const {
    ASSERT(entryIndex < numInterruptSources());
    u64 const lowDword(readRegister(redirectionTableEntryRegLow(entryIndex)));
    u64 const highDword(readRegister(redirectionTableEntryRegHigh(entryIndex)));
    return (highDword << 32) | lowDword;
}

// Write an entry into the redirection table.
// @param entryIndex: The index of the entry to write.
// @param value: The value the entry should be set to.
void IoApic::writeRedirectionTable(u8 const entryIndex,
                                   RedirectionTableEntry const entry) {
    ASSERT(entryIndex < numInterruptSources());
    // Writing into a 64-bit register MUST to be done by first writing the low
    // DWORD followed by the high DWORD.
    // Also avoid overwritting reserved bits.
    u64 const entryRaw(entry.raw());
    u64 const currRaw(readRedirectionTable(entryIndex).raw());
    u64 const newRaw((entryRaw & ~RedirectionTableEntryReservedBits)
                     | (currRaw & RedirectionTableEntryReservedBits));
    writeRegister(redirectionTableEntryRegLow(entryIndex), newRaw & 0xffffffff);
    writeRegister(redirectionTableEntryRegHigh(entryIndex), newRaw >> 32);
}
}
