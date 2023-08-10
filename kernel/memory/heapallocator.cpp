#include "heapallocator.hpp"
#include <framealloc/framealloc.hpp>
#include <paging/paging.hpp>
#include <util/assert.hpp>

namespace HeapAlloc {

// Instantiate a heap allocator.
// @param heapStart: The starting virtual address for the heap managed by this
// allocator.
// @param maxHeapSize: The maximum size in bytes for this heap. If the heap
// grows to this size, any allocation requests that requires more physical
// memory for the heap will fail.
// @param frameAllocator: The custom frame allocator to use when the heap
// requires more physical memory. By default uses FrameAlloc::alloc.
HeapAllocator::HeapAllocator(VirAddr const heapStart,
                             u64 const maxHeapSize,
                             FrameAllocator const frameAllocator) :
    m_heapStart(heapStart), m_maxHeapSize(maxHeapSize), m_heapSize(0),
    m_frameAllocator(frameAllocator) {
    ASSERT(!(maxHeapSize % PAGE_SIZE));
}

// Allocate memory from this heap.
// @param size: The size of the allocation in bytes.
// @return: If the allocation is successful returns a void* to the allocated
// memory. Otherwise returns an Error.
Res<void*> HeapAllocator::alloc(u64 const size) {
    u64 const allocSize(size + sizeof(Metadata));
    // The allocation may take a couple of tries if it cannot fit in the current
    // heap, hence the loop.
    while (true) {
        Res<VirAddr> const allocRes(m_freeList.alloc(allocSize));
        if (allocRes.ok()) {
            // Allocation was successful, prepare the metadata block.
            // Address of the allocated memory.
            VirAddr const ret(allocRes.value() + sizeof(Metadata));
            Metadata * const metadata(allocRes->ptr<Metadata>());
            metadata->size = size;
            metadata->token = ret.raw() ^ Metadata::MagicNumber;
            return ret.ptr<void>();
        } else {
            // The allocation failed due to the fact that there is not enough
            // free space in the heap. Allocate more physical memory and map it
            // to the end of the heap.
            if (m_heapSize + PAGE_SIZE > m_maxHeapSize) {
                // Allocating one more page would go over the limit, fail the
                // allocation.
                Log::crit("Cannot grow heap, max heap size reached");
                return Error::MaxHeapSizeReached;
            }
            Log::info("Growing heap to {} bytes", m_heapSize + PAGE_SIZE);
            Res<Frame> const frameAllocRes(m_frameAllocator());
            if (!frameAllocRes) {
                Log::crit("Could not allocate frame for heap allocator");
                // FIXME: Need a constructor in Res<T> taking an Err as arg.
                return frameAllocRes.error();
            }
            // Map the new frame to the end of the current heap.
            PhyAddr const framePhyAddr(frameAllocRes->phyOffset());
            VirAddr const mappedAddr(m_heapStart.raw() + m_heapSize);
            Err const err(Paging::map(mappedAddr, framePhyAddr, 1));
            if (!!err) {
                Log::crit("Could not map new frame for heap allocator");
                // FIXME: Need a constructor in Res<T> taking an Err as arg.
                return err.error();
            }
            m_heapSize += PAGE_SIZE;
            // Update the freelist to contain the new page added to the heap.
            m_freeList.insert(mappedAddr, PAGE_SIZE);
            // Re-try the allocation in the next iteration.
            Log::debug("Re-trying heap allocation of {} bytes", allocSize);
        }
    }
}

// Free memory from this heap.
// @param ptr: void* to the memory that should be freed. This pointer should
// have come from a call to alloc() on this same HeapAllocator.
void HeapAllocator::free(void const * const ptr) {
    VirAddr const allocAddr(ptr);
    VirAddr const metadataVAddr(allocAddr - sizeof(Metadata));
    Metadata * const metadata(metadataVAddr.ptr<Metadata>());
    // Check that the token matches the expected value.
    u64 const expToken(allocAddr.raw() ^ Metadata::MagicNumber);
    if (metadata->token != expToken) {
        PANIC("Calling HeapAllocator::free with a non matching token. This is "
              "most likely a double-free or freeing memory that was not "
              "allocated using HeapAlloc::malloc()");
    }
    m_freeList.insert(metadataVAddr, metadata->size + sizeof(Metadata)); 
}

}
