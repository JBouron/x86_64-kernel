// Frame allocator interface and implementation types.
#include <framealloc/framealloc.hpp>
#include "freelist.hpp"

namespace FrameAlloc {

// Abstract interface for a frame allocator.
class Allocator {
public:
    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function returns an error.
    virtual Res<Frame> alloc() = 0;

    // Free a physical frame.
    // @param frame: The Frame describing the physical frame to be freed.
    virtual void free(Frame const& frame) = 0;
};

// Forward declaration needed by EarlyAllocator.
class EmbeddedFreeListAllocator;

// During early kernel intialization we do not have a heap allocator and nor do
// we have a mapping of the entire physical memory to high addresses. Those two
// things need a frame allocator to be initialized creating a chicken-and-egg
// problem. However, a frame allocator that is not allowed to use heap
// allocations or access physical memory is not very useful. This is why we
// first start with an "early allocator": a dead simple allocator that is just
// enough to initialize the heap allocator and the page tables mapping physical
// memory to high addresses. Once those are setup we can switch to a
// better/smarter frame allocator.
// This early allocator uses the physical frame free-list coming from the
// BootStruct, it is only able to allocate, but not free frames. This is fine as
// the frames allocated by this allocator are not expected to be ever freed
// (e.g. page tables for physical memory mapping, ...).
class EarlyAllocator : public Allocator {
public:
    // Create an early allocator using the free-list coming from the bootStruct.
    // @param bootStruct: The bootStruct from which the free-list is taken.
    EarlyAllocator(BootStruct const& bootStruct);

    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function returns an error.
    virtual Res<Frame> alloc();

    // Free a physical frame. This operation is not implemented by this
    // allocator (see comment above class definition) and as such panics.
    virtual void free(Frame const& frame);

    // Initialize an EmbeddedFreeListAllocator's free-list with this allocator's
    // free list. This is used as a "handover" situation when switching from the
    // EarlyAllocator to the EmbeddedFreeListAllocator once paging and the
    // direct map have been initialized.
    void initEmbeddedFreeListAllocator(EmbeddedFreeListAllocator& alloc) const;

private:
    // The free-list entry in which the next allocation should take place.
    BootStruct::PhyFrameFreeListNode const * m_nextAllocNode;
    // The index of the next page to be allocated in m_nextAllocNode.
    u64 m_nextAllocFrameIndex;
};

// The "real" frame allocator to replace the EarlyAllocator once paging and the
// direct map have been initialized.
// As with the EarlyAllocator, this allocator uses an free-list to keep track of
// free pages. However, unlike the EarlyAllocator, the free-list here is
// embedded, that is stored within the free physical frames.
// The EmbeddedFreeListAllocator only deals with virtual addresses, using the
// direct map to access the physical frames.
class EmbeddedFreeListAllocator : public Allocator {
public:
    // Create an empty EmbeddedFreeListAllocator. An EmbeddedFreeListAllocator
    // is meant to be constructed iteratively.
    EmbeddedFreeListAllocator();

    // Add a region of free frames to the allocator. Note: this will write the
    // into the frame starting at addr. This function can only be called before
    // calling alloc/free for the first time.
    // @param addr: The start vaddr of this region.
    // @param numFrames: The number of frames in the region.
    void insertFreeRegion(VirAddr const& addr, u64 const numFrames);

    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function returns an error.
    virtual Res<Frame> alloc();

    // Free a physical frame. This operation is not implemented by this
    // allocator (see comment above class definition) and as such panics.
    virtual void free(Frame const& frame);

private:
    // Free-list of physical page frames.
    EmbeddedFreeList m_freeList;

    // Indicate if a call to insertFreeRegion() is currently allowed. Once
    // alloc() or free() is called, insertFreeRegion() is not longer allowed to
    // be called and PANICs.
    bool m_allowInsert;
};
}
