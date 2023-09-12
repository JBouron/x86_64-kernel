// Frame allocator interface and implementation types.
#include <util/panic.hpp>
#include <util/assert.hpp>

#include "allocator.hpp"

namespace FrameAlloc {
// Create an early allocator using the free-list coming from the bootStruct.
// @param bootStruct: The bootStruct from which the free-list is taken.
EarlyAllocator::EarlyAllocator(BootStruct const& bootStruct) : 
    m_nextAllocNode(bootStruct.phyFrameFreeListHead),
    m_nextAllocFrameIndex(0) {}

// Allocate a new physical frame.
// @return: The Frame object describing the allocated frame. If no frame can be
// allocated this function returns an error.
Res<Frame> EarlyAllocator::alloc() {
    // Frame allocation in the EarlyAllocator is simple: each allocation returns
    // the first available frame in the free-list. To keep track of the next
    // available frame we keep track of the current node (m_nextAllocNode) and
    // the index of the next free frame in that node (m_nextAllocFrameIndex).
    if (!m_nextAllocNode) {
        return Error::OutOfPhysicalMemory;
    }
    // At this point, m_nextAllocNode is guaranteed to have at least one
    // available frame.
    ASSERT(m_nextAllocFrameIndex < m_nextAllocNode->numFrames);
    Frame const res(m_nextAllocNode->base +
                    m_nextAllocFrameIndex * PAGE_SIZE);
    m_nextAllocFrameIndex++;
    if (m_nextAllocFrameIndex == m_nextAllocNode->numFrames) {
        // We allocated all the frames in the current node, go to the next node
        // for the next allocation.
        m_nextAllocNode = m_nextAllocNode->next;
        m_nextAllocFrameIndex = 0;
    }
    return res;
}

// Free a physical frame. This operation is not implemented by this allocator
// (see comment above class definition) and as such panics.
void EarlyAllocator::free(Frame const& frame) {
    // free() is not implemented in this allocator.
    PANIC("Attempted to free physical frame {}: not implemented", frame.addr());
}

// Initialize an EmbeddedFreeListAllocator's free-list with this allocator's
// free list. This is used as a "handover" situation when switching from the
// EarlyAllocator to the EmbeddedFreeListAllocator once paging and the direct
// map have been initialized.
void EarlyAllocator::initEmbeddedFreeListAllocator(
    EmbeddedFreeListAllocator& alloc) const {
    if (!m_nextAllocNode) {
        // This is technically not a critical situation as there could always be
        // some frames being freed after initializing the
        // EmbeddedFreeListAllocator.
        Log::warn("EarlyAllocator's free-list is empty during handover");
    }
    BootStruct::PhyFrameFreeListNode const * node(m_nextAllocNode);
    while (!!node) {
        // Some frames in the m_nextAllocNode might have been allocated already,
        // make sure we don't insert them as free frames in the
        // EmbeddedFreeListAllocator.
        u64 const allocatedInNode(
            (node == m_nextAllocNode) ? m_nextAllocFrameIndex : 0);
        VirAddr const baseVAddr(
            PhyAddr(node->base + allocatedInNode * PAGE_SIZE).toVir());
        ASSERT(allocatedInNode < node->numFrames);
        u64 const numFrames(node->numFrames - allocatedInNode);
        alloc.insertFreeRegion(baseVAddr, numFrames);
        node = node->next;
    }
}

// Create an empty EmbeddedFreeListAllocator. An EmbeddedFreeListAllocator is
// meant to be constructed iteratively.
EmbeddedFreeListAllocator::EmbeddedFreeListAllocator() : m_allowInsert(true) {}

// Add a region of free frames to the allocator. Note: this will write the into
// the frame starting at addr. This function can only be called before calling
// alloc/free for the first time.
// @param addr: The start vaddr of this region.
// @param numFrames: The number of frames in the region.
void EmbeddedFreeListAllocator::insertFreeRegion(VirAddr const& addr,
                                                 u64 const numFrames) {
    if (!m_allowInsert) {
        PANIC("Called insertFreeRegion() after alloc() or free()");
    }
    m_freeList.insert(addr, numFrames * PAGE_SIZE);
}

// Allocate a new physical frame.
// @return: The Frame object describing the allocated frame. If no frame can be
// allocated this function returns an error.
Res<Frame> EmbeddedFreeListAllocator::alloc() {
    Res<VirAddr> const allocResult(m_freeList.alloc(PAGE_SIZE));
    if (!allocResult) {
        return allocResult.error();
    } else {
        return allocResult.value().raw() - Paging::DIRECT_MAP_START_VADDR;
    }
}

// Free a physical frame. This operation is not implemented by this allocator
// (see comment above class definition) and as such panics.
void EmbeddedFreeListAllocator::free(Frame const& frame) {
    m_freeList.free(frame.addr().toVir(), PAGE_SIZE);
}

}
