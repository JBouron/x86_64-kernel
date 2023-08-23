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

    // FIXME: The RSDT and its SDTs are located at high physical addresses that
    // are not contained in the direct map. We have two options here:
    //  1. Make sure the Direct Map contains the _entire_ physical memory.
    //  2. Map and un-map each table as we use them.
    // For now map the frame on which the RSDT is, by chance the SDTs are on the
    // same frame. This needs to be fixed ASAP!
    PhyAddr const paddr(rsdtAddr.raw() & ~(PAGE_SIZE - 1));
    VirAddr const vaddr(paddr.raw());
    ASSERT(!Paging::map(vaddr, paddr, Paging::PageAttr::None, 1));

    Rsdt const * const rsdt(VirAddr(rsdtAddr.raw()).ptr<Rsdt>());
    if (!rsdt->header.isValid()) {
        PANIC("RSDT has an invalid checksum");
    }

    u64 const numTables(rsdt->numTables());
    Log::info("RSDT contains {} tables:", numTables);
    for (u64 i(0); i < numTables; ++i) {
        PhyAddr const sdtPAddr(rsdt->sdtPointers[i]);
        RsdtHeader const * const hdr(VirAddr(sdtPAddr.raw()).ptr<RsdtHeader>());
        char const sig[5] = { hdr->signature[0], hdr->signature[1],
            hdr->signature[2], hdr->signature[3], 0 };
        Log::info("  {}: @{}", sig, sdtPAddr);
    }

    // TODO: Parse the tables of interest and store their information somewhere.
}
}
