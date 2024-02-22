// Types and definition related to address spaces.

#include <paging/addrspace.hpp>
#include <framealloc/framealloc.hpp>
#include <paging/paging.hpp>
#include <cpu/cpu.hpp>
#include <util/cstring.hpp>
#include <logging/log.hpp>

namespace Paging {
// Create a new address space. The new address space shares the mapping of
// kernel addresses used by the current address space. The user addresses are
// un-mapped.
// @return: A Ptr to the new AddrSpace or an error if any.
Res<Ptr<AddrSpace>> AddrSpace::New() {
    Res<Frame> const pml4Alloc(FrameAlloc::alloc());
    if (!pml4Alloc) {
        return pml4Alloc.error();
    }

    // The pml4 for the new address space.
    PhyAddr const pml4(pml4Alloc.value().addr());

    // Copy the current address space's mapping for the kernel addresses that is
    // the second half of the pml4. The first half of the pml4, ie the user
    // portion are left un-mapped.
    // User addresses.
    Util::memzero(pml4.toVir().ptr<void>(), PAGE_SIZE / 2);

    // Kernel addresses.
    PhyAddr const currPml4(Cpu::cr3() & ~(PAGE_SIZE - 1));
    u8 const * const src(currPml4.toVir().ptr<u8>() + (PAGE_SIZE / 2));
    u8 * const dst(pml4.toVir().ptr<u8>() + (PAGE_SIZE / 2));
    Util::memcpy(dst, src, PAGE_SIZE / 2);

    Ptr<AddrSpace> const allocRes(Ptr<AddrSpace>::New(pml4));
    // FIXME: Add shortcut for this error handling.
    if (!allocRes) {
        return Error::MaxHeapSizeReached;
    } else {
        return allocRes;
    }
}

static void clearPageTable(u8 const level, PhyAddr const table) {
    // FIXME: We may be better off doing this in paging.cpp with the
    // PageTable<L> class.
    ASSERT(level > 0);
    ASSERT(table.isPageAligned());
    if (level > 1) {
        u64 const * entry(table.toVir().ptr<u64>());
        u64 const maxIdx(level == 4 ? 256 : 512);
        for (u64 i(0); i < maxIdx; ++i) {
            bool const isPresent(entry[i] & 1);
            if (isPresent) {
                PhyAddr const nextTableAddr(entry[i] & ~(PAGE_SIZE - 1));
                clearPageTable(level - 1, nextTableAddr);
            }
        }
    }
    Log::debug("Deallocating page-table level {} at {}", level, table);
    FrameAlloc::free(Frame(table.raw()));
}

// De-allocate part of the page-table structure used by this AddrSpace that is
// not shared with other AddrSpace (ie. the user mapping).
AddrSpace::~AddrSpace() {
    clearPageTable(4, m_pml4Address);
}

// Get the address of the PML4 associated with this address space.
// @return: The physical address of the PML4.
PhyAddr AddrSpace::pml4Address() const {
    return m_pml4Address;
}

// Switch from the current address space to another one.
// @param to: The address space to switch to.
void AddrSpace::switchAddrSpace(Ptr<AddrSpace> const& to) {
    switchAddrSpace(to->m_pml4Address);
}

// This overload is mostly meant for testing.
void AddrSpace::switchAddrSpace(PhyAddr const& pml4) {
    u64 const currCr3(Cpu::cr3());
    u64 const pml4AddrMask(~(PAGE_SIZE - 1));
    u64 const newCr3((currCr3 & ~pml4AddrMask) | (pml4.raw() & pml4AddrMask));
    Cpu::writeCr3(newCr3);
}

// Create an AddrSpace.
// @param pml4: Physical address of the top-level page table.
AddrSpace::AddrSpace(PhyAddr const pml4) : m_pml4Address(pml4) {}
}
