// Definition of the EmbeddedFreeList class.
#pragma once
#include <paging/paging.hpp>

namespace FrameAlloc {

// An embedded free list is a singly linked list of free memory regions where
// the list nodes are stored within the free regions, not allocated elsewhere,
// hence the term "embedded".
// An EmbeddedFreeList is typically used for physical frame allocations, however
// the class is written to work with allocations of arbitrary sizes of byte
// granularity, this makes testing easier as we don't need to reserve actual
// physical frames to test the list.
class EmbeddedFreeList {
public:
    // Create an empty EmbeddedFreeList.
    EmbeddedFreeList();

    // Insert a region of free memory in the EmbeddedFreeList. This is mostly
    // used to iteratively construct the free list. Note: since Nodes are
    // embedded, this function will write at address `startAddr`, in other words
    // this function gives ownership of this region of memory to the free list.
    // @param startAddr: The start virtual address of the region of free memory
    // to be inserted in the free-list.
    // @param size: The size of the region of free memory.
    void insert(VirAddr const startAddr, u64 const size);

    // Allocate memory from the free-list.
    // @param size: The size of the allocation in bytes.
    // @return: The virtual address of the allocated memory.
    // FIXME: Figure out what to do in case the memory cannot be allocated.
    VirAddr alloc(u64 const size);

    // Free memory that was allocated from this free-list, adds this memory back
    // to the free-list. The address passed as argument *must* have come from a
    // call to alloc().
    // @param addr: The address of the memory region to be freed.
    // @param size: The size of the memory region in bytes.
    void free(VirAddr const addr, u64 const size);

private:
    // A node in the singly linked list, representing an region of free memory.
    // This region of memory starts at the address of this Node structure.
    struct Node {
        // The size of the region in bytes.
        u64 size;

        // Pointer to the next node in the free list. If this node is the last
        // node in the list then this is nullptr.
        Node * next;
        
        // Use fromVirAddr instead.
        Node() = delete;

        // Construct a node for the memory region starting at `addr` of `size`
        // bytes.
        // @param addr: The starting virtual address of the region of free
        // memory.
        // @param size: The size of the memory region in bytes.
        // @return: A pointer to the Node representing this region of free
        // memory. The size is initialized from `size` and `next` is set to
        // nullptr.
        static Node* fromVirAddr(VirAddr const addr, u64 const size);

        // Get the base address of this memory region, that is the address of
        // the first byte contained in the region.
        VirAddr base() const;

        // Get the end address of this memory region, that is the address of
        // the last byte contained in the region.
        VirAddr end() const;

        // Check if this memory region overlaps with another. That is if there
        // exists at least one byte that is present in both regions.
        // @param other: The other Node to check overlap with.
        // @return: true if this Node and other are overlapping, false
        // otherwise.
        bool overlapsWith(Node const& other) const;

        // Check if this memory region is adjacent with another. That is if this
        // Node immediately precedes or immediately follows the other Node.
        // @param other: The other Node to check adjacency with.
        // @return: true if those Nodes are adjacent, false otherwise.
        bool adjacentWith(Node const& other) const;
    };

    // Pointer to the first Node of the free list. nullptr if the freelist is
    // empty.
    Node* m_head;

    // Make the EmbeddedFreeList tests as friend to be able to test the internal
    // state of the free-list.
    friend SelfTests::TestResult embeddedFreeListNodeTest();
    friend SelfTests::TestResult embeddedFreeListNodeOverlapTest();
    friend SelfTests::TestResult embeddedFreeListNodeAdjacentWithTest();
    friend SelfTests::TestResult embeddedFreeListInsertTest();
    friend SelfTests::TestResult embeddedFreeListAllocFreeTest();
};

}
