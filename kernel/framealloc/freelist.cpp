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
    Node * const newNode(Node::fromVirAddr(startAddr, size));

    Node** prevNextPtr(&m_head);
    Node* curr(m_head);
    while (!!curr) {
        // The node being inserted in the free list may not overlap with any
        // other node. If this assert fails then we have a double free
        // situation.
        ASSERT(!curr->overlapsWith(*newNode));
        // FIXME: Write VirAddr comparison operators.
        if (newNode->base().raw() < curr->base().raw()) {
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
// @return: The virtual address of the allocated memory.
// FIXME: Figure out what to do in case the memory cannot be allocated.
VirAddr EmbeddedFreeList::alloc(u64 const size) {
    // FIXME: There oughta be a minimum allocation size.
    Node** prevNext(&m_head);
    Node* curr(m_head);
    while (!!curr) {
        if (size <= curr->size) {
            // We can make the allocation in the current node. Allocate at the
            // end of this node so that we only need to change the size and we
            // don't need to copy the Node metadata around.
            VirAddr const res(curr->end().raw() - size + 1);
            curr->size -= size;
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

    PANIC("Empty free list");
    UNREACHABLE
}

// Free memory that was allocated from this free-list, adds this memory back to
// the free-list. The address passed as argument *must* have come from a call to
// alloc().
// @param addr: The address of the memory region to be freed.
// @param size: The size of the memory region in bytes.
void EmbeddedFreeList::free(VirAddr const addr, u64 const size) {
    // Re-use insert() for the free.
    insert(addr, size);
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
    return reinterpret_cast<u64>(this);
}

// Get the end address of this memory region, that is the address of the last
// byte contained in the region.
VirAddr EmbeddedFreeList::Node::end() const {
    return reinterpret_cast<u64>(this) + size - 1;
}

// Check if this memory region overlaps with another. That is if there exists at
// least one byte that is present in both regions.
// @param other: The other Node to check overlap with.
// @return: true if this Node and other are overlapping, false otherwise.
bool EmbeddedFreeList::Node::overlapsWith(Node const& other) const {
    // FIXME: Write VirAddr comparison operators.
    u64 const tBase(base().raw());
    u64 const tEnd(end().raw());
    u64 const oBase(other.base().raw());
    u64 const oEnd(other.end().raw());
    return (oBase <= tBase && tBase <= oEnd) ||
           (oBase <= tEnd && tEnd <= oEnd) ||
           (tBase <= oBase && oEnd <= tEnd);
}

// Check if this memory region is adjacent with another. That is if this Node
// immediately precedes or immediately follows the other Node.
// @param other: The other Node to check adjacency with.
// @return: true if those Nodes are adjacent, false otherwise.
bool EmbeddedFreeList::Node::adjacentWith(Node const& other) const {
    // FIXME: Write VirAddr comparison operators.
    u64 const tBase(base().raw());
    u64 const tEnd(end().raw());
    u64 const oBase(other.base().raw());
    u64 const oEnd(other.end().raw());
    return tEnd == oBase - 1 || oEnd == tBase - 1;
}

}
