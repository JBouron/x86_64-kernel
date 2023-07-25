#include <framealloc/framealloc.hpp>
#include <util/panic.hpp>
#include <util/assert.hpp>

namespace FrameAlloc {

// Create a Frame from its physical offset.
// @param physicalOffset: The physical offset of the frame.
Frame::Frame(u64 const physicalOffset) : m_physicalOffset(physicalOffset) {}

// Get the physical offset of the frame.
u64 Frame::phyOffset() const {
    return m_physicalOffset;
}

// Abstract interface for a frame allocator.
class Allocator {
public:
    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function should panic. FIXME: Eventually we should
    // handle OOM situation better than a panic.
    virtual Frame alloc() = 0;

    // Free a physical frame.
    // @param frame: The Frame describing the physical frame to be freed.
    virtual void free(Frame const& frame) = 0;
};

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
    EarlyAllocator(BootStruct const& bootStruct) : 
        m_nextAllocNode(bootStruct.phyFrameFreeListHead),
        m_nextAllocFrameIndex(0) {}

    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function should panic. FIXME: Eventually we should
    // handle OOM situation better than a panic.
    virtual Frame alloc() {
        // Frame allocation in the EarlyAllocator is simple: each allocation
        // returns the first available frame in the free-list. To keep track of
        // the next available frame we keep track of the current node
        // (m_nextAllocNode) and the index of the next free frame in that node
        // (m_nextAllocFrameIndex).
        if (!m_nextAllocNode) {
            PANIC("No physical frame available for allocation");
        }
        // At this point, m_nextAllocNode is guaranteed to have at least one
        // available frame.
        ASSERT(m_nextAllocFrameIndex < m_nextAllocNode->numFrames);
        Frame const res(m_nextAllocNode->base +
                        m_nextAllocFrameIndex * PAGE_SIZE);
        m_nextAllocFrameIndex++;
        if (m_nextAllocFrameIndex == m_nextAllocNode->numFrames) {
            // We allocated all the frames in the current node, go to the next
            // node for the next allocation.
            m_nextAllocNode = m_nextAllocNode->next;
        }
        return res;
    }

    // Free a physical frame. This operation is not implemented by this
    // allocator (see comment above class definition) and as such panics.
    virtual void free(Frame const& frame) {
        // free() is not implemented in this allocator.
        u64 const offset(frame.phyOffset());
        PANIC("Attempted to free physical frame @{}: not implemented", offset);
    }

private:
    // The free-list entry in which the next allocation should take place.
    BootStruct::PhyFrameFreeListNode const * m_nextAllocNode;
    // The index of the next page to be allocated in m_nextAllocNode.
    u64 m_nextAllocFrameIndex;
};

// The current instance of the global frame allocator.
static Allocator* GLOBAL_ALLOCATOR = nullptr;

// Initialize the frame allocator.
// @param bootStruct: The bootStruct passed by the bootloader. The frame
// allocator is initialized from the bootStruct's physical frame free list.
void Init(BootStruct const& bootStruct) {
    if (!!GLOBAL_ALLOCATOR) {
        Log::warn("FrameAlloc::Init called twice, skipping");
        return;
    }
    static EarlyAllocator earlyAllocator(bootStruct);
    GLOBAL_ALLOCATOR = &earlyAllocator;
    Log::debug("Initialized early frame allocator");
}

// Allocate a physical frame using the global allocator. This function panics if
// no frame can be allocated.
// @return: The Frame describing the allocated frame.
Frame alloc() {
    if (!GLOBAL_ALLOCATOR) {
        PANIC("Attempt to call FrameAlloc::alloc() before calling Init()!");
    }
    return GLOBAL_ALLOCATOR->alloc();
}

// Free an allocated physical frame.
// @param Frame: A Frame describing the physical frame to be freed.
void free(Frame const& frame) {
    if (!GLOBAL_ALLOCATOR) {
        PANIC("Attempt to call FrameAlloc::free() before calling Init()!");
    }
    GLOBAL_ALLOCATOR->free(frame);
}

}
