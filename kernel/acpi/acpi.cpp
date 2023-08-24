// Functions to parse the ACPI tables.
#include <acpi/acpi.hpp>
#include <paging/paging.hpp>
#include <util/assert.hpp>
#include "tables.hpp"

namespace Acpi {

// FIXME: This file is probably full of undefined behaviour due to having to
// parse structs from raw memory. I should really take the time to double check
// everything here...

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
            break;
        }
        case Madt::Entry::Type::IoApic: {
            u8 const ioApicId(entry->read<u8>(2));
            u32 const ioApicAddr(entry->read<u32>(4));
            u32 const intBase(entry->read<u32>(8));
            Log::info("      [{}]: IO APIC: IO APIC ID = {} IO APIC addr = {x}"
                      " int base = {}", idx, ioApicId, ioApicAddr, intBase);
            break;
        }
        case Madt::Entry::Type::InterruptSourceOverride: {
            u8 const busSrc(entry->read<u8>(2));
            u8 const irqSrc(entry->read<u8>(3));
            u32 const gis(entry->read<u32>(4));
            u16 const flags(entry->read<u16>(8));
            Log::info("      [{}]: Int src override: Bus src = {} IRQ src = {}"
                      " GIS = {} flags = {}", idx, busSrc, irqSrc, gis, flags);
            break;
        }
        case Madt::Entry::Type::NMISource: {
            u8 const nmiSource(entry->read<u8>(2));
            u16 const flags(entry->read<u16>(3));
            u8 const gis(entry->read<u8>(5));
            Log::info("      [{}]: NMI src: src = {} flags = {} GIS = {}",
                      idx, nmiSource, flags, gis);
            break;
        }
        case Madt::Entry::Type::LocalApicNMI: {
            u8 const procId(entry->read<u8>(2));
            u16 const flags(entry->read<u16>(3));
            u8 const lint(entry->read<u8>(5));
            Log::info("      [{}]: LAPIC NMI : cpuID = {} flags = {} LINT = {}",
                      idx, procId, flags, lint);
            break;
        }
        case Madt::Entry::Type::LocalApicAddressOverride: {
            u64 const lapicAddr(entry->read<u64>(8));
            Log::info("      [{}]: LAPIC override: LAPIC addr = {x}",
                      idx, lapicAddr);
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
    madt->forEachEntry(parseMadtEntry);
}

// Parse the ACPI tables found in BIOS memory.
void parseTables() {
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

    // TODO: Parse the tables of interest and store their information somewhere.
}
}
