// Functions to parse the ACPI tables.
#include <acpi/acpi.hpp>
#include <paging/paging.hpp>
#include <util/assert.hpp>
#include "tables.hpp"

namespace Acpi {

// FIXME: This file is probably full of undefined behaviour due to having to
// parse structs from raw memory. I should really take the time to double check
// everything here...


// FIXME: The following constants define the maximum size of the various arrays
// of the Info struct. Those are are needed because we don't have dynamically
// sized arrays yet and use statically sized arrays instead. Eventually, once we
// have dynamic arrays, we won't need those anymore.
static constexpr u64 MAX_IO_APIC = 4;
static constexpr u64 MAX_CPUS = 256;
static constexpr u64 MAX_NMI_SOURCES = 4;

// The Info struct parsed from the ACPI tables.
static Info AcpiInfo;

// Indicate if the ACPI info was already parsed. Used to avoid parsing the ACPI
// tables multiple times.
static bool Parsed = false;

// Compare two signatures. Note: both signatures MUST be of the same length as
// specified by signatureLen.
// @param arr1: The first signature.
// @param arr2: The second signature.
// @param signatureLen: The length of both signatures in number of chars.
// @return: true if both signatures are identical, false otherwise.
static bool compareSignatures(char const * const arr1,
                              char const * const arr2,
                              u8 const signatureLen) {
    for (u64 i(0); i < signatureLen; ++i) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

// Search the RSDP in a particular memory range.
// @param searchStartAddr: The start address to search from. Must be 16-byte
// aligned.
// @param searchStopAddr: The end address.
// @return: If the RSDP is found this function returns its physical address
// (which is in the direct map) otherwise it returns an error.
static Res<PhyAddr> searchRsdpInRange(PhyAddr const searchStartAddr,
                                      PhyAddr const searchStopAddr) {
    ASSERT(searchStartAddr < searchStopAddr);
    ASSERT(!(searchStartAddr.raw() & 0xf));
    // The RSDP is always on a 16 byte boundary.
    for (PhyAddr addr(searchStartAddr); addr < searchStopAddr; addr = addr+16) {
        Rsdp const * const candidate(Paging::toVirAddr(addr).ptr<Rsdp>());
        if (compareSignatures(candidate->signature, "RSD PTR ", 8)) {
            return addr;
        }
    }
    return Error::NoRsdpFound;
}

// Search for the RSDP in the standard locations.
// @return: If found, return the physical address of the RSDP, otherwise return
// an error.
static Res<PhyAddr> findRsdp() {
    PhyAddr const ebdaBase(u64(*Paging::toVirAddr(0x040E).ptr<u16>()) << 4);
    Log::info("EBDA Base = {}", ebdaBase);

    // Search the first 1KiB of the EBDA
    Res<PhyAddr> const fromEbda(searchRsdpInRange(ebdaBase, ebdaBase + 1024));
    if (fromEbda) {
        // FIXME: Need a copy constructor in Res<T>.
        return fromEbda.value();
    }

    // If the EBDA fails then search the region 0xe0000 - 0xfffff.
    Res<PhyAddr> const secondTry(searchRsdpInRange(0xe0000, 0xfffff));
    if (secondTry) {
        return secondTry.value();
    }

    // If we are still not find the table then brute force and search from 0 to
    // 1MiB.
    Res<PhyAddr> const thirdTry(searchRsdpInRange(0x0, 0xfffff));
    if (thirdTry) {
        return thirdTry.value();
    }

    // Couldn't find the table.
    return Error::NoRsdpFound;
}

// Translate a raw value of MPS INTI flags (see ACPI specs) into a Polarity.
// @param flags: The value to translate.
// @return: The polarity encoded in the flags.
static Info::Polarity mpsIntiFlagsToPolarity(u16 const flags) {
    u8 const bits(flags & 0x2);
    if (!bits) {
        return Info::Polarity::ConformToBusSpecs;
    } else if (bits == 1) {
        return Info::Polarity::ActiveHigh;
    } else if (bits == 3) {
        return Info::Polarity::ActiveLow;
    } else {
        PANIC("Invalid MPS INTI flag value: {}", flags);
    }
}

// Translate a raw value of MPS INTI flags (see ACPI specs) into a TriggerMode.
// @param flags: The value to translate.
// @return: The TriggerMode encoded in the flags.
static Info::TriggerMode mpsIntiFlagsToTriggerMode(u16 const flags) {
    u8 const bits(flags >> 2);
    if (!bits) {
        return Info::TriggerMode::ConformToBusSpecs;
    } else if (bits == 1) {
        return Info::TriggerMode::EdgeTriggered;
    } else if (bits == 3) {
        return Info::TriggerMode::LevelTriggered;
    } else {
        PANIC("Invalid MPS INTI flag value: {}", flags);
    }
}

// Parse an Entry in the MADT. Used by parseMadt as a callback for
// Madt::forEachEntry.
// @param idx: Index of the entry in the MADT.
// @param Entry: Pointer to the entry to be parsed.
static void parseMadtEntry(u64 const idx, Madt::Entry const * const entry) {
    switch (entry->type) {
        case Madt::Entry::Type::ProcessorLocalApic: {
            u8 const procId(entry->read<u8>(2));
            u8 const apicId(entry->read<u8>(3));
            u32 const flags(entry->read<u32>(4));
            Log::info("      [{}]: LAPIC: CPU ID = {} APIC ID = {} flags = {}",
                      idx, procId, apicId, flags);
            if (procId >= MAX_CPUS) {
                PANIC("More cpus than currently supported. Change MAX_CPUS");
            }
            Info::ProcessorDesc& desc(AcpiInfo.processorDesc[procId]);
            desc.id = procId;
            desc.apicId = apicId;
            desc.isEnabled = !!(flags & 0x1);
            desc.isOnlineCapable = !!(flags & 0x2);
            desc.hasNmiSource = false;
            AcpiInfo.processorDescSize++;
            break;
        }
        case Madt::Entry::Type::IoApic: {
            u8 const ioApicId(entry->read<u8>(2));
            u32 const ioApicAddr(entry->read<u32>(4));
            u32 const intBase(entry->read<u32>(8));
            Log::info("      [{}]: IO APIC: IO APIC ID = {} IO APIC addr = {x}"
                      " int base = {}", idx, ioApicId, ioApicAddr, intBase);
            if (ioApicId >= MAX_IO_APIC) {
                PANIC("More I/O APICs than supported. Change MAX_IO_APIC");
            }
            Info::IoApicDesc& desc(AcpiInfo.ioApicDesc[ioApicId]);
            desc.id = ioApicId;
            desc.address = ioApicAddr;
            desc.interruptBase = GlobalSystemIntVector(intBase);
            AcpiInfo.ioApicDescSize++;
            break;
        }
        case Madt::Entry::Type::InterruptSourceOverride: {
            u8 const busSrc(entry->read<u8>(2));
            u8 const irqSrc(entry->read<u8>(3));
            u32 const gsi(entry->read<u32>(4));
            u16 const flags(entry->read<u16>(8));
            Log::info("      [{}]: Int src override: Bus src = {} IRQ src = {}"
                      " gsi = {} flags = {}", idx, busSrc, irqSrc, gsi, flags);
            // FIXME: Not sure what the bus src is for.
            ASSERT(!busSrc);
            ASSERT(irqSrc <= 15);
            Info::IrqDesc& desc(AcpiInfo.irqDesc[irqSrc]);
            desc.gsiVector = GlobalSystemIntVector(gsi);
            desc.polarity = mpsIntiFlagsToPolarity(flags);
            desc.triggerMode = mpsIntiFlagsToTriggerMode(flags);
            break;
        }
        case Madt::Entry::Type::NMISource: {
            u8 const nmiSource(entry->read<u8>(2));
            u16 const flags(entry->read<u16>(3));
            u8 const gsi(entry->read<u8>(5));
            Log::info("      [{}]: NMI src: src = {} flags = {} gsi = {}",
                      idx, nmiSource, flags, gsi);
            if (AcpiInfo.nmiSourceDescSize >= MAX_NMI_SOURCES) {
                PANIC("More NMI sources than supported. Change MAX_NMI_SOURCE");
            }
            u8 const idx(AcpiInfo.nmiSourceDescSize);
            Info::NmiSourceDesc& desc(AcpiInfo.nmiSourceDesc[idx]);
            desc.polarity = mpsIntiFlagsToPolarity(flags);
            desc.triggerMode = mpsIntiFlagsToTriggerMode(flags);
            desc.gsiVector = GlobalSystemIntVector(gsi);
            AcpiInfo.nmiSourceDescSize++;
            break;
        }
        case Madt::Entry::Type::LocalApicNMI: {
            u8 const procId(entry->read<u8>(2));
            u16 const flags(entry->read<u16>(3));
            u8 const lint(entry->read<u8>(5));
            Log::info("      [{}]: LAPIC NMI : cpuID = {} flags = {} LINT = {}",
                      idx, procId, flags, lint);
            if (procId >= MAX_CPUS) {
                PANIC("Proc ID from LocalApicNMI is out of bounds");
            }
            Info::ProcessorDesc& desc(AcpiInfo.processorDesc[procId]);
            desc.hasNmiSource = true;
            desc.nmiPolarity = mpsIntiFlagsToPolarity(flags);
            desc.nmiTriggerMode = mpsIntiFlagsToTriggerMode(flags);
            desc.nmiLint = lint;
            break;
        }
        case Madt::Entry::Type::LocalApicAddressOverride: {
            u64 const lapicAddr(entry->read<u64>(8));
            Log::info("      [{}]: LAPIC override: LAPIC addr = {x}",
                      idx, lapicAddr);
            AcpiInfo.localApicAddress = lapicAddr;
            break;
        }
        default: break;
    }
}

// Parse a MADT.
// @param madt: Pointer to the madt to parse.
static void parseMadt(Madt const * const madt) {
    Log::info("    Local APIC Address = {x}", madt->localApicPhyAddr);
    Log::info("    Flags = {x}", madt->flags);
    Log::info("    Entries:");

    AcpiInfo.localApicAddress = madt->localApicPhyAddr;
    AcpiInfo.hasDual8259 = madt->flags & 1;

    madt->forEachEntry(parseMadtEntry);
}

// Parse the ACPI tables found in BIOS memory.
Info const& parseTables() {
    if (Parsed) {
        // Tables were already parsed, don't parse them again.
        return AcpiInfo;
    }

    // Prepare the arrays in the Info struct.
    AcpiInfo.processorDesc = new Info::ProcessorDesc[MAX_CPUS];
    AcpiInfo.ioApicDesc = new Info::IoApicDesc[MAX_IO_APIC];
    AcpiInfo.nmiSourceDesc = new Info::NmiSourceDesc[MAX_NMI_SOURCES];

    // Set all IRQs to be ID-mapped, edge-triggered and active-high (e.g. they
    // standard behaviour).
    for (u8 irq(0); irq < 15; ++irq) {
        Info::IrqDesc& desc(AcpiInfo.irqDesc[irq]);
        desc.gsiVector = GlobalSystemIntVector(irq);
        desc.polarity = Info::Polarity::ActiveHigh;
        desc.triggerMode = Info::TriggerMode::EdgeTriggered;
    }

    Log::info("Parsing ACPI tables:");
    Res<PhyAddr> const rsdpLoc(findRsdp());
    if (!rsdpLoc) {
        PANIC("Could not find RSDP");
    }
    Log::info("RSDP found @{}", *rsdpLoc);

    Rsdp const * const rsdp(Paging::toVirAddr(*rsdpLoc).ptr<Rsdp>());
    if (!rsdp->isValid()) {
        PANIC("The RSDP has an invalid checksum");
    } else if (!!rsdp->revision) {
        // For now we only support ACPI v1.0 as it is the default revision
        // created by Qemu. On real hardware however we might need support for
        // >= v2.0.
        PANIC("Unsupported RSDP revision {}", rsdp->revision);
    }
    PhyAddr const rsdtAddr(rsdp->rsdtAddress);
    Log::info("RSDT is @{}", rsdtAddr);

    // Validate the RSDT checksum.
    Rsdt const * const rsdt(Paging::toVirAddr(rsdtAddr).ptr<Rsdt>());
    if (!rsdt->header.isValid()) {
        PANIC("RSDT has an invalid checksum");
    }

    // RSDT is valid, parse the SDTs it contains.
    u64 const numTables(rsdt->numTables());
    Log::info("RSDT contains {} tables:", numTables);
    for (u64 i(0); i < numTables; ++i) {
        Sdt const * const sdt(rsdt->table(i));
        char const * const sdtSig(sdt->header.signature);
        char const sig[5] = {sdtSig[0], sdtSig[1], sdtSig[2], sdtSig[3], 0};
        if (!sdt->header.isValid()) {
            PANIC("Table {} has invalid checksum!", sig);
        }
        Log::info("  {} table @{}", sig, sdt);
        if (compareSignatures(sdtSig, "APIC", 4)) {
            parseMadt(reinterpret_cast<Madt const*>(sdt));
        } else {
            Log::info("    Ignored by this kernel");
        }
    }
    return AcpiInfo;
}
}
