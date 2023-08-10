// Definition of a heap allocator using an underlying EmbeddedFreeList.
#pragma once

#include <framealloc/framealloc.hpp>
#include <datastruct/freelist.hpp>

namespace HeapAlloc {

// A heap allocator. This allocator lazily allocate physical frames as needed
// and maps them starting at its given heap start address.
class HeapAllocator {
public:
    // Type of a function allocating a physical page frame.
    using FrameAllocator = Res<Frame>(*)();

    // Instantiate a heap allocator.
    // @param heapStart: The start virtual address for the heap managed by this
    // allocator.
    // @param maxHeapSize: The maximum size in bytes for this heap. If the heap
    // grows to this size, any allocation request that requires more physical
    // memory for the heap will fail.
    // @param frameAllocator: The custom frame allocator to use when the heap
    // requires more physical memory. By default uses FrameAlloc::alloc.
    HeapAllocator(VirAddr const heapStart,
                  u64 const maxHeapSize,
                  FrameAllocator const frameAllocator = FrameAlloc::alloc);

    // Allocate memory from this heap.
    // @param size: The size of the allocation in bytes.
    // @return: If the allocation is successful returns a void* to the allocated
    // memory. Otherwise returns an Error.
    Res<void*> alloc(u64 const size);

    // Free memory from this heap.
    // @param ptr: void* to the memory that should be freed. This pointer should
    // come from a call to alloc() on this same HeapAllocator.
    void free(void const * const ptr);

private:
    // Each allocation of N bytes on the heap is preceeded by a Metadata block
    // which contains information about the allocation itself. Therefore
    // allocating N bytes is, in reality, allocating N + sizeof(Metadata) bytes.
    struct Metadata {
        // The size, in bytes, of the allocation immediately following this
        // Metadata block. /!\ This does not include the bytes for the metadata
        // block itself!
        u64 size;

        // Magic number used to compute the per-allocation token. See comment
        // below.
        static const u64 MagicNumber = 0x1412041b14140207;

        // Token computed as the XOR of MagicNumber and the address of the
        // allocation immediately following this Metadata. This is used by the
        // free() function to verify (with decent probability) that the pointer
        // to be freed has indeed been allocated by the heap AND is currently
        // refering to allocated/non-free memory.
        u64 token;
    };

    // Start virtual address of the heap managed by this allocator.
    VirAddr const m_heapStart;

    // The maximum size of the heap managed by this allocator.
    u64 const m_maxHeapSize;

    // The current size of the heap in bytes. This reflect the amount of memory
    // reserved for the entire heap, therefore counting both allocated and free
    // memory within the heap.
    u64 m_heapSize;

    // The frame allocator to be used.
    FrameAllocator const m_frameAllocator;

    // The freelist of the heap.
    DataStruct::EmbeddedFreeList m_freeList;

    friend SelfTests::TestResult heapAllocatorTest();
};
}
