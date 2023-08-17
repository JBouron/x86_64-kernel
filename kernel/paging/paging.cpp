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
    Res<Frame> const allocRes(FrameAlloc::alloc());
    if (!allocRes) {
        // There is nothing we can do in case of a failure at this point.
        PANIC("Failed to allocate frame while initializing direct map");
    }
    Frame const& frame(allocRes.value());
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

    // Enable the Write-Protect bit on CR0 to catch writes on read-only pages
    // originating from ring 0.
    Cpu::writeCr0(Cpu::cr0() | (1 << 16));
}

// operator| for PageAttr. Use to create combination of attributes.
// @param attr1: The first attr.
// @param attr2: The second attr.
// @return: The OR of attr1 and attr2.
PageAttr operator|(PageAttr const& attr1, PageAttr const& attr2) {
    // This is apparently safe to do as long as we specify from which type the
    // enum is deriving from, in our case u64. This is a weird C++ quirk ...
    return PageAttr(static_cast<u64>(attr1) | static_cast<u64>(attr2));
}

// operator& for PageAttr. Used to test that a combination of attribute
// (computed using operator| for instance) contains a particular attribute.
// @param attr1: The first attr.
// @param attr2: The second attr.
// @return: true if attr1 and attr2 share at least one bit/flag, false
// otherwise.
bool operator&(PageAttr const& attr1, PageAttr const& attr2) {
    // This is apparently safe to do as long as we specify from which type the
    // enum is deriving from, in our case u64. This is a weird C++ quirk ...
    return !!(static_cast<u64>(attr1) & static_cast<u64>(attr2));
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
    // @param vaddr: The virtual address to map.
    // @param paddr: The physical address to map vaddr to.
    // @param attrs: The attributes of the mapping.
    // @return: Returns an error if the mapping failed.
    Err map(VirAddr const vaddr, PhyAddr const paddr, PageAttr const attrs) {
        u16 const idx((vaddr.raw() >> (12 + (L-1) * 9)) & 0x1ff);
        Entry& entry(entries[idx]);
        if constexpr (L == 1) {
            entry.present = true;
            entry.writable = attrs & PageAttr::Writable;
            entry.userAccessible = attrs & PageAttr::User;
            entry.writeThrough = attrs & PageAttr::WriteThrough;
            entry.cacheDisable = attrs & PageAttr::CacheDisable;
            entry.global = attrs & PageAttr::Global;
            entry.executeDisable = attrs & PageAttr::NoExec;
            entry.addr = paddr.raw() >> 12;
        } else {
            if (!entry.present) {
                Res<Frame> const allocRes(FrameAlloc::alloc());
                if (!allocRes) {
                    return allocRes.error();
                }
                entry.present = true;
                // For the upper levels, set the writable and user bit to true
                // so that the PTE at the last level decides if a given page is
                // writable/user accessible.
                entry.writable = true;
                entry.userAccessible = true;
                entry.addr = allocRes->phyOffset() >> 12;
            }
            VirAddr const nextLevelVaddr(toVirAddr(entry.addr << 12));
            PageTable<L-1>* nextLevel(nextLevelVaddr.ptr<PageTable<L-1>>());
            nextLevel->map(vaddr, paddr, attrs);
        }
        return Ok;
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
// @param pageAttr: Control the attribute of the mapping. All mapped pages will
// end up using those attributes.
// @param nPages: The size of the region in number of pages.
// @return: Returns an error if the mapping failed.
Err map(VirAddr const vaddrStart,
        PhyAddr const paddrStart,
        PageAttr const pageAttr,
        u64 const nPages) {
    ASSERT(vaddrStart.isPageAligned());
    ASSERT(paddrStart.isPageAligned());
    ASSERT(!!nPages);
    Log::debug("Mapping {} to {} ({} pages)", vaddrStart, paddrStart, nPages);
    VirAddr const pml4VAddr(toVirAddr(Cpu::cr3() & ~(PAGE_SIZE - 1)));
    PageTable<4>* pml4(pml4VAddr.ptr<PageTable<4>>());
    Err returnedErr;
    for (u64 i(0); i < nPages; ++i) {
        VirAddr const vaddr(vaddrStart.raw() + i * PAGE_SIZE);
        PhyAddr const paddr(paddrStart.raw() + i * PAGE_SIZE);
        Err const err(pml4->map(vaddr, paddr, pageAttr));
        if (err) {
            // Stop on the first error. This does mean that the request might
            // have been partially completed, e.g. we mapped half of the pages.
            // FIXME: Define better semantics, either we yolo this and have
            // undefined result on error OR we un-map the pages that were
            // successfully mapped before the error occured.
            returnedErr = err;
            break;
        }
    }
    // Reload CR3.
    Cpu::writeCr3(Cpu::cr3());
    return returnedErr;
}

}
