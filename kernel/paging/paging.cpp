// Everything related to paging.

#include <paging/paging.hpp>
#include <framealloc/framealloc.hpp>
#include <logging/log.hpp>
#include <util/assert.hpp>
#include <cpu/cpu.hpp>

namespace Paging {

// Initialize the direct map. Implemented in assembly because me thinks this is
// actually faster than C++.
// @param directMapStartAddr: Where to start the direct map in the virtual
// address space. This is essentially the vaddr corresponding to paddr 0x0.
// @param maxPhyAddr: The maximum physical address/offset to map in the direct
// map. This address is _not_ included in the map.
extern "C" void initializeDirectMap(u64 const directMapStartAddr,
                                    u64 const maxPhyAddr);

// Indicate the max physical offset that has been mapped to the direct map so
// far. Updated from initializeDirectMap while constructing the direct map, used
// by the allocFrameFromAssembly function to know whether or not to return an ID
// mapped address or Direct mapped address.
extern "C" {
    u64 directMapMaxMappedOffset = 0;
}

// Helper function for the assembly function initializeDirectMap. Allocate a
// physical frame using the frame allocator. Using "C" linkage so we don't
// bother with name mangling.
// @return: The address of the allocated frame. This is either a physical
// address OR a direct mapped address depending on whether or not the frame is
// contained in the physical addresses that are memory mapped already (e.g.
// under directMapMaxMappedOffset).
extern "C" u64 allocFrameFromAssembly() {
    Frame const frame(FrameAlloc::alloc());
    u64 const offset(frame.phyOffset());
    if (offset <= directMapMaxMappedOffset) {
        return offset + DIRECT_MAP_START_VADDR;
    } else {
        return offset;
    }
}

// Get the virtual address in the direct map corresponding to the given physical
// address.
// @param paddr: The physical address to translate.
// @return: The direct map address mapped to `paddr`.
VirAddr toVirAddr(PhyAddr const paddr) {
    return VirAddr(paddr.raw() + DIRECT_MAP_START_VADDR);
}

// Initialize paging.
// This function creates the direct map.
void Init(BootStruct const& bootStruct) {
    // To find the end of physical memory, we use the last byte of the last
    // entry of available memory in the e820 memory map.
    u64 dmEndOffset(0);
    for (u64 i(0); i < bootStruct.memoryMapSize; ++i) {
        BootStruct::MemMapEntry const& entry(bootStruct.memoryMap[i]);
        if (entry.isAvailable()) {
            u64 const entryEndOffset(entry.base + entry.length);
            dmEndOffset = max(dmEndOffset, entryEndOffset);
        }
    }
    Log::info("Initializing direct map spanning {x} bytes", dmEndOffset);
    initializeDirectMap(DIRECT_MAP_START_VADDR, dmEndOffset);
    Log::debug("Direct map initialized");
}

// Page-table types are taking the level of the table L as template parameter
// using the following mapping:
//  Level 4 => PML4
//  Level 3 => Page Directory Pointer table
//  Level 2 => Page Directory
//  Level 1 => Page Table
// Level 0 is therefore the page itself.

// Type of an entry in a level L page table.
template<u8 L> requires (0 < L && L <= 4)
struct PageTableEntry {
    u8 present : 1;
    u8 writable : 1;
    u8 userAccessible : 1;
    u8 writeThrough : 1;
    u8 cacheDisable : 1;
    u8 accessed : 1;
    u16 : 6;
    u64 addr : 51;
    u8 executeDisable : 1;
} __attribute__((packed));

// Specialization for level 1 tables (e.g. "Page Table"). Level 1 as a few more
// bits than the other levels such as dirty and global.
template<>
struct PageTableEntry<1> {
    u8 present : 1;
    u8 writable : 1;
    u8 userAccessible : 1;
    u8 writeThrough : 1;
    u8 cacheDisable : 1;
    u8 accessed : 1;
    u8 dirty : 1;
    u8 pat : 1;
    u8 global : 1;
    u16 : 3;
    u64 addr : 51;
    u8 executeDisable : 1;
} __attribute__((packed));

// Type of a level L page table.
template<u8 L> requires (0 < L && L <= 4)
struct PageTable {
    // Entry type for this table.
    using Entry = PageTableEntry<L>;
    static_assert(sizeof(Entry) == sizeof(u64));

    // Map vaddr to paddr. If this is a level 1 table, the associated entry is
    // directly modified. Otherwise this method recurse to the level L-1 table,
    // allocating it if necessary.
    // FIXME: Add a way to control the attributes of the mapping.
    // @param vaddr: The virtual address to map.
    // @param paddr: The physical address to map vaddr to.
    void map(VirAddr const vaddr, PhyAddr const paddr) {
        u16 const idx((vaddr.raw() >> (12 + (L-1) * 9)) & 0x1ff);
        Entry& entry(entries[idx]);
        if constexpr (L == 1) {
            entry.present = true;
            entry.writable = true;
            entry.addr = paddr.raw() >> 12;
        } else {
            if (!entry.present) {
                Frame const nextLevel(FrameAlloc::alloc());
                entry.present = true;
                entry.writable = true;
                entry.addr = nextLevel.phyOffset() >> 12;
            }
            VirAddr const nextLevelVaddr(toVirAddr(entry.addr << 12));
            PageTable<L-1>* nextLevel(nextLevelVaddr.ptr<PageTable<L-1>>());
            nextLevel->map(vaddr, paddr);
        }
    }

private:
    // The entries of this page table.
    Entry entries[512];
} __attribute__((packed));

// Sanity check that we got the sizes right.
static_assert(sizeof(PageTable<4>) == PAGE_SIZE);
static_assert(sizeof(PageTable<3>) == PAGE_SIZE);
static_assert(sizeof(PageTable<2>) == PAGE_SIZE);
static_assert(sizeof(PageTable<1>) == PAGE_SIZE);

// Map a region of virtual memory to physical memory in the current address
// space. The region's size must be a multiple of page size.
// @param vaddrStart: The start virtual address of the region to be mapped. Must
// be page aligned.
// @param paddrStart: The start physical address at which the region should be
// mapped. Must be page aligned.
// @param nPages: The size of the region in number of pages.
void map(VirAddr const vaddrStart, PhyAddr const paddrStart, u64 const nPages) {
    ASSERT(vaddrStart.isPageAligned());
    ASSERT(paddrStart.isPageAligned());
    ASSERT(!!nPages);
    Log::debug("Mapping {x} to {x} ({} pages)",
               vaddrStart.raw(),
               paddrStart.raw(),
               nPages);
    VirAddr const pml4VAddr(toVirAddr(Cpu::cr3() & ~(PAGE_SIZE - 1)));
    PageTable<4>* pml4(pml4VAddr.ptr<PageTable<4>>());
    for (u64 i(0); i < nPages; ++i) {
        VirAddr const vaddr(vaddrStart.raw() + i * PAGE_SIZE);
        PhyAddr const paddr(paddrStart.raw() + i * PAGE_SIZE);
        pml4->map(vaddr, paddr);
    }
}

}
