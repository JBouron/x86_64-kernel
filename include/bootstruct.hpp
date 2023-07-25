// C++ side of the bootstruct definition. This definition must be kept
// consistent with the fields/offsets/sizes used in
// bootloader/stage1/bootstruct.asm.
#pragma once

#include <util/util.hpp>

struct BootStruct {
    // Entry in the e820 memory map. This defines a physical memory region in
    // the range [base; base + length[.
    // The type indicates if the region is available for use or reserved (e.g.
    // hardware mapped, bad RAM, and countless other reasons, ...).
    struct MemMapEntry {
        // The physical address at which the region starts. This address is not
        // necessarily page-aligned.
        u64 const base;
        // The length, in bytes, of this memory region. This value is not
        // necessarily a multiple of page size.
        u64 const length;
        // The type of the entry. 1 means available for use while any value
        // other than 1 usually means reserved.
        u64 const type;

        // Helper function to check if this entry denotes available ram.
        // @return: true if the memory within this region is available for use,
        // false otherwise.
        bool isAvailable() const { return type == 1; }
    } __attribute__((packed));

    // Pointer to the e820 memory map. The bootloader sanitized this memory map
    // to provide the following guarantees:
    //  - The memory map is sorted on base address.
    //  - No two entries in the memory map are overlapping.
    MemMapEntry const * const memoryMap;

    // The size of the memory map, in number of entries.
    u64 const memoryMapSize;

    // A node in the physical frame free list created by the bootloader. This
    // list indicates all physical frames that are available for use per the
    // e820 memory map but not currently in use.
    // Each node describes a region of physical memory consisting of one or more
    // free frames.
    struct PhyFrameFreeListNode {
        // The base address of the region of free memory. This address is
        // page-aligned.
        u64 const base;
        // The number of free frames in this region. All free frames of a region
        // are continuous.
        u64 const numFrames;
        // Pointer to the next node in the free list.
        PhyFrameFreeListNode const * const next;
    };

    // Pointer to the first node in the physical frame free list.
    PhyFrameFreeListNode const * const phyFrameFreeListHead;
} __attribute__((packed));
static_assert(sizeof(BootStruct) == 3 * sizeof(u64));
