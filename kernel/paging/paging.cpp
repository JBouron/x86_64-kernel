// Everything related to paging.

#include <paging/paging.hpp>
#include <framealloc/framealloc.hpp>
#include <logging/log.hpp>
#include <util/assert.hpp>

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

// Initialize paging.
// This function creates the direct map.
void Init(BootStruct const& bootStruct) {
    // To find the end of physical memory, we use the last byte of the last
    // entry of the e820 memory map.
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

}
