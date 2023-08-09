#include "freelist.hpp"
#include <util/assert.hpp>

namespace FrameAlloc {

// Create an empty EmbeddedFreeList.
EmbeddedFreeList::EmbeddedFreeList() : m_head(nullptr) {}

// Insert a region of free memory in the EmbeddedFreeList. This is mostly used
// to iteratively construct the free list. Note: since Nodes are embedded, this
// function will write at address `startAddr`, in other words this function
// gives ownership of this region of memory to the free list.
// @param startAddr: The start virtual address of the region of free memory to
// be inserted in the free-list.
// @param size: The size of the region of free memory.
void EmbeddedFreeList::insert(VirAddr const startAddr, u64 const size) {
    ASSERT(MinAllocSize <= size);
    Node * const newNode(Node::fromVirAddr(startAddr, size));
    Node** prevNextPtr(&m_head);
    Node* curr(m_head);
    while (!!curr) {
        // The node being inserted in the free list may not overlap with any
        // other node. If this assert fails then we have a double free
        // situation.
        ASSERT(!curr->overlapsWith(*newNode));
        if (newNode->base() < curr->base()) {
            // The new node either needs to be inserted before curr or be merged
            // with curr and replace curr.
            if (newNode->adjacentWith(*curr)) {
                // Need to merge with curr. Since newNode's base is < than curr
                // base we merge curr into newNode.
                newNode->size += curr->size;
                newNode->next = curr->next;
            } else {
                // Cannot merge, just insert newNode before curr.
                newNode->next = curr;
            }
            *prevNextPtr = newNode;
            return;
        } else if (curr->adjacentWith(*newNode)) {
            // Curr base is < newNode base and they are adjacent, merge newNode
            // into curr.
            curr->size += newNode->size;
            // Now that we grew curr we might have opened an opportunity to
            // merge curr and curr->next.
            if (!!curr->next && curr->adjacentWith(*curr->next)) {
                // Merge curr and curr->next.
                curr->size += curr->next->size;
                // Remove curr->next from the list.
                curr->next = curr->next->next;
            }
            return;
        }

        prevNextPtr = &curr->next;
        curr = curr->next;
    }
    // We could not find a node with a base address higher than that of newNode,
    // and no node is adjacent to newNode, therefore insert newNode at the end
    // of the list.
    *prevNextPtr = newNode;
    newNode->next = nullptr;
}

// Allocate memory from the free-list.
// @param size: The size of the allocation in bytes.
// @return: The virtual address of the allocated memory. If the allocation
// failed then return an Error.
Res<VirAddr> EmbeddedFreeList::alloc(u64 const size) {
    // Honor the minimum allocation size.
    u64 const allocSize(max(MinAllocSize, size));
    Node** prevNext(&m_head);
    Node* curr(m_head);
    while (!!curr) {
        u64 const sizeAfterAlloc(curr->size - allocSize);
        bool const canHoldAlloc(
            allocSize <= curr->size
            && (!sizeAfterAlloc || MinAllocSize <= sizeAfterAlloc));
        // The allocation is only possible in the current node if it is big
        // enough to hold the allocation AND if there is enough space left after
        // the allocation to hold the new Node metadata. The exception is the
        // case where the allocation consumes exactly the same number of bytes
        // currently available in that node in which case we don't need to
        // create a new Node.
        if (canHoldAlloc) {
            // We can make the allocation in the current node. Allocate at the
            // end of this node so that we only need to change the size and we
            // don't need to copy the Node metadata around.
            VirAddr const res(curr->end() - allocSize + 1);
            curr->size -= allocSize;
            if (!curr->size) {
                // Remove node due to being empty.
                *prevNext = curr->next;
            }
            // FIXME: We should probably zero the allocated memory here.
            return res;
        }

        prevNext = &curr->next;
        curr = curr->next;
    }
    return Error::OutOfPhysicalMemory;
}

// Free memory that was allocated from this free-list, adds this memory back to
// the free-list. The address passed as argument *must* have come from a call to
// alloc().
// @param addr: The address of the memory region to be freed.
// @param size: The size of the memory region in bytes.
void EmbeddedFreeList::free(VirAddr const addr, u64 const size) {
    // Re-use insert() for the free.
    u64 const allocSize(max(MinAllocSize, size));
    insert(addr, allocSize);
}

// Construct a node for the memory region starting at `addr` of `size` bytes.
// @param addr: The starting virtual address of the region of free memory.
// @param size: The size of the memory region in bytes.
// @return: A pointer to the Node representing this region of free memory. The
// size is initialized from `size` and `next` is set to nullptr.
EmbeddedFreeList::Node* EmbeddedFreeList::Node::fromVirAddr(VirAddr const addr,
                                                            u64 const size) {
    Node* const node(addr.ptr<Node>());
    node->size = size;
    node->next = nullptr;
    return node;
}

// Get the base address of this memory region, that is the address of the first
// byte contained in the region.
VirAddr EmbeddedFreeList::Node::base() const {
    return this;
}

// Get the end address of this memory region, that is the address of the last
// byte contained in the region.
VirAddr EmbeddedFreeList::Node::end() const {
    return reinterpret_cast<u8 const*>(this) + size - 1;
}

// Check if this memory region overlaps with another. That is if there exists at
// least one byte that is present in both regions.
// @param other: The other Node to check overlap with.
// @return: true if this Node and other are overlapping, false otherwise.
bool EmbeddedFreeList::Node::overlapsWith(Node const& other) const {
    VirAddr const& tBase(base());
    VirAddr const& tEnd(end());
    VirAddr const& oBase(other.base());
    VirAddr const& oEnd(other.end());
    return (oBase <= tBase && tBase <= oEnd) ||
           (oBase <= tEnd && tEnd <= oEnd) ||
           (tBase <= oBase && oEnd <= tEnd);
}

// Check if this memory region is adjacent with another. That is if this Node
// immediately precedes or immediately follows the other Node.
// @param other: The other Node to check adjacency with.
// @return: true if those Nodes are adjacent, false otherwise.
bool EmbeddedFreeList::Node::adjacentWith(Node const& other) const {
    VirAddr const& tBase(base());
    VirAddr const& tEnd(end());
    VirAddr const& oBase(other.base());
    VirAddr const& oEnd(other.end());
    return tEnd == oBase - 1 || oEnd == tBase - 1;
}

}
