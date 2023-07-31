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
    EmbeddedFreeListAllocator() : m_head(nullptr), m_allowInsert(true) {}

    // Add a region of free frames to the allocator. Note: this will write the
    // into the frame starting at addr. This function can only be called before
    // calling alloc/free for the first time.
    // @param addr: The start vaddr of this region.
    // @param numFrames: The number of frames in the region.
    void insertFreeRegion(VirAddr const& addr, u64 const numFrames) {
        if (!m_allowInsert) {
            PANIC("Called insertFreeRegion() after alloc() or free()");
        }

        // Initialize the new embedded entry.
        Entry * const newEntry(addr.ptr<Entry>());
        newEntry->numFrames = numFrames;
        newEntry->next = nullptr;

        // Update the next pointer of the last entry in the free-list.
        Entry ** insertDest(&m_head);
        while (!!*insertDest && !!(*insertDest)->next) {
            insertDest = &(*insertDest)->next;
        }
        *insertDest = newEntry;
    }

    // Allocate a new physical frame.
    // @return: The Frame object describing the allocated frame. If no frame can
    // be allocated this function should panic. FIXME: Eventually we should
    // handle OOM situation better than a panic.
    virtual Frame alloc() {
        VirAddr const res(m_head->lastFrame());
        // Update the counter on head.
        if (!(--m_head->numFrames)) {
            // This was the last frame in the head entry, update head to point
            // to head's next.
            m_head = m_head->next;
        }
        return Frame(res.raw() - Paging::DIRECT_MAP_START_VADDR);
    }

    // Free a physical frame. This operation is not implemented by this
    // allocator (see comment above class definition) and as such panics.
    virtual void free(Frame const& frame) {
        VirAddr const frameVAddr(Paging::toVirAddr(frame.phyOffset()));
        // Insert in the free-list.
        Entry * const frameEntry(frameVAddr.ptr<Entry>());
        Entry * ptr(m_head);
        Entry ** toUpdate(&m_head);
        while (!!ptr && ptr < frameEntry) {
            toUpdate = &ptr->next;
            ptr = ptr->next;
        }
        ASSERT(ptr != frameEntry);
        frameEntry->numFrames = 1;
        frameEntry->next = ptr;
        *toUpdate = frameEntry;
        // TODO: Implement Entry merging.
    }

private:
    // An entry in the free list. This contain the metadata about a region of
    // continuous physical frames. This struct is stored starting at the first
    // byte of the first frame of this region, this is why there is no `base`
    // member, the base is `this`.
    struct Entry {
        // The number of free frames in this region.
        u64 numFrames;
        // Pointer to the next entry.
        Entry *next;

        // Return a pointer to the last available frame in this region.
        VirAddr lastFrame() const {
            u64 const thisPtr(reinterpret_cast<u64>(this));
            return thisPtr + (numFrames - 1) * PAGE_SIZE;
        }
    };

    // Pointer to the head entry of the free-list.
    Entry *m_head;

    // Indicate if a call to insertFreeRegion() is currently allowed. Once
    // alloc() or free() is called, insertFreeRegion() is not longer allowed to
    // be called and PANICs.
    bool m_allowInsert;
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

    // Initialize an EmbeddedFreeListAllocator's free-list with this allocator's
    // free list. This is used as a "handover" situation when switching from the
    // EarlyAllocator to the EmbeddedFreeListAllocator once paging and the
    // direct map have been initialized.
    void initEmbeddedFreeListAllocator(EmbeddedFreeListAllocator& alloc) const {
        if (!m_nextAllocNode) {
            // This is technically not a critical situation as there could
            // always be some frames being freed after initializing the
            // EmbeddedFreeListAllocator.
            Log::warn("EarlyAllocator's free-list is empty during handover");
        }
        BootStruct::PhyFrameFreeListNode const * node(m_nextAllocNode);
        while (!!node) {
            // Some frames in the m_nextAllocNode might have been allocated
            // already, make sure we don't insert them as free frames in the
            // EmbeddedFreeListAllocator.
            u64 const allocatedInNode(
                (node == m_nextAllocNode) ? m_nextAllocFrameIndex : 0);
            VirAddr const baseVAddr(
                Paging::toVirAddr(node->base + allocatedInNode * PAGE_SIZE));
            ASSERT(allocatedInNode < node->numFrames);
            u64 const numFrames(node->numFrames - allocatedInNode);
            alloc.insertFreeRegion(baseVAddr, numFrames);
            node = node->next;
        }
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

// Notify the frame allocator that the direct map has been initialized.
void directMapInitialized() {
    static bool embAllocatorInit = false;
    if (embAllocatorInit) {
        Log::warn("FrameAlloc::directMapInitialized called twice, skipping");
        return;
    }
    static EmbeddedFreeListAllocator embAllocator;
    // FIXME: Remove this cast.
    EarlyAllocator const* const earlyAlloc(
        static_cast<EarlyAllocator const*>(GLOBAL_ALLOCATOR));
    earlyAlloc->initEmbeddedFreeListAllocator(embAllocator);
    GLOBAL_ALLOCATOR = &embAllocator;
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
