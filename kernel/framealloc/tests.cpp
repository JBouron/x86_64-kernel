// Test for FrameAlloc namespace.
#include <framealloc/framealloc.hpp>
#include <util/assert.hpp>
#include <bootstruct.hpp>
#include "allocator.hpp"
#include "freelist.hpp"

namespace FrameAlloc {

// Test the EarlyAllocator.
SelfTests::TestResult earlyAllocatorTest() {
    // First step is to create a dummy BootStruct that will be used by a dummy
    // EarlyAllocator. The EarlyAllocator only uses the phyFrameFreeListHead
    // field of the BootStruct so we can get away with only setting this field.
    BootStruct::PhyFrameFreeListNode const node3({
        .base = 0x30000,
        .numFrames = 3,
        .next = nullptr,
    });
    BootStruct::PhyFrameFreeListNode const node2({
        .base = 0x20000,
        .numFrames = 2,
        .next = &node3,
    });
    BootStruct::PhyFrameFreeListNode const node1({
        .base = 0x10000,
        .numFrames = 1,
        .next = &node2,
    });
    BootStruct::PhyFrameFreeListNode const node0({
        .base = 0x00000,
        .numFrames = 1,
        .next = &node1,
    });
    BootStruct const bootstruct({
        .memoryMap = nullptr,
        .memoryMapSize = 0,
        .phyFrameFreeListHead = &node0,
    });

    // The EarlyAllocator being tested.
    EarlyAllocator allocator(bootstruct);

    TEST_ASSERT(allocator.alloc().phyOffset() == 0x0);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x10000);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x20000);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x21000);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x30000);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x31000);
    TEST_ASSERT(allocator.alloc().phyOffset() == 0x32000);
    // FIXME: Once we gracefully handle the OOM case, we shoud add a test case
    // that the next call to alloc() fails.
    return SelfTests::TestResult::Success;
}

// Test constructing an EmbeddedFreeList::Node from a virtual address and a
// size.
SelfTests::TestResult embeddedFreeListNodeTest() {
    u8 buf[64] = {0};
    EmbeddedFreeList::Node const * const node(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf), 64));
    TEST_ASSERT(node->base().raw() == reinterpret_cast<u64>(buf));
    TEST_ASSERT(node->end().raw() == reinterpret_cast<u64>(buf) + 64 - 1);
    TEST_ASSERT(node->size == 64);
    return SelfTests::TestResult::Success;
}

// Check the overlapsWith function.
SelfTests::TestResult embeddedFreeListNodeOverlapTest() {
    u8 buf[64];
    EmbeddedFreeList::Node const * const node1(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf), 32));
    EmbeddedFreeList::Node const * const node2(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+32), 32));
    TEST_ASSERT(node1->overlapsWith(*node1));
    TEST_ASSERT(node2->overlapsWith(*node2));
    TEST_ASSERT(!node1->overlapsWith(*node2));
    TEST_ASSERT(!node2->overlapsWith(*node1));
    EmbeddedFreeList::Node const * const node3(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+31), 32));
    TEST_ASSERT(node1->overlapsWith(*node3));
    TEST_ASSERT(node3->overlapsWith(*node1));
    TEST_ASSERT(node2->overlapsWith(*node3));
    TEST_ASSERT(node3->overlapsWith(*node2));
    EmbeddedFreeList::Node const * const node4(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+8), 16));
    TEST_ASSERT(node1->overlapsWith(*node4));
    TEST_ASSERT(node4->overlapsWith(*node1));
    return SelfTests::TestResult::Success;
}

// Check the adjacentWith function.
SelfTests::TestResult embeddedFreeListNodeAdjacentWithTest() {
    u8 buf[64];
    // Positive tests.
    EmbeddedFreeList::Node const * const node1(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf), 16));
    EmbeddedFreeList::Node const * const node2(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+16), 16));
    TEST_ASSERT(node1->adjacentWith(*node2));
    TEST_ASSERT(node2->adjacentWith(*node1));
    // Negative tests.
    EmbeddedFreeList::Node const * const node3(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+16), 16));
    EmbeddedFreeList::Node const * const node4(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf), 15));
    EmbeddedFreeList::Node const * const node5(
        EmbeddedFreeList::Node::fromVirAddr(reinterpret_cast<u64>(buf+33), 16));
    TEST_ASSERT(!node3->adjacentWith(*node4));
    TEST_ASSERT(!node3->adjacentWith(*node5));
    TEST_ASSERT(!node4->adjacentWith(*node3));
    TEST_ASSERT(!node5->adjacentWith(*node3));
    return SelfTests::TestResult::Success;
}

// Check the insert function of EmbeddedFreeList.
SelfTests::TestResult embeddedFreeListInsertTest() {
    // Create a free list on a buffer of 256 bytes. Each node of this list has
    // the same size.
    u64 const bufSize(256);
    u8 buf[bufSize];
    u64 const numNodes(4);
    ASSERT(!(bufSize % numNodes));
    u64 const nodeSize(bufSize / numNodes);
    EmbeddedFreeList freeList;

    // Insert even elems.
    for (u64 i(0); i < numNodes; i += 2) {
        freeList.insert(reinterpret_cast<u64>(buf + nodeSize * i), nodeSize);
    }

    EmbeddedFreeList::Node* const first(freeList.m_head);
    TEST_ASSERT(first->size == nodeSize);
    TEST_ASSERT(!!first->next);
    EmbeddedFreeList::Node* const next(first->next);
    TEST_ASSERT(next->size == nodeSize);
    TEST_ASSERT(!next->next);

    // Insert odd nodes.
    for (u64 i(1); i < numNodes; i += 2) {
        freeList.insert(reinterpret_cast<u64>(buf + nodeSize * i), nodeSize);
    }

    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    return SelfTests::TestResult::Success;
}

// Somewhat end-to-end test where we build a freelist and call alloc() and
// free() on it.
SelfTests::TestResult embeddedFreeListAllocFreeTest() {
    u64 const bufSize(256);
    u8 buf[bufSize];
    EmbeddedFreeList freeList;

    // Initialize the free list with the full buffer as free.
    freeList.insert(reinterpret_cast<u64>(buf), bufSize);
    // Sanity check.
    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Make multiple allocations so that we allocate the entire buffer.
    u64 const numAllocs(4);
    ASSERT(!(bufSize % numAllocs));
    u64 const allocSize(bufSize / numAllocs);

    VirAddr allocations[4];
    for (u64 i(0); i < numAllocs; ++i) {
        allocations[i] = freeList.alloc(allocSize);
    }

    // Sanity check: The head of the EmbeddedFreeList should be nullptr.
    TEST_ASSERT(!freeList.m_head);

    // FIXME: Once we handle the empty free list case properly we need to add a
    // test case that the next call to alloc() fails.

    // Check that no two calls to alloc() returned the same address and that
    // each address' offset from the buffer is a multiple of the allocation
    // size.
    VirAddr const bufVirAddr(reinterpret_cast<u64>(buf));
    for (u64 i(0); i < numAllocs; ++i) {
        VirAddr const& addr(allocations[i]);
        TEST_ASSERT(bufVirAddr.raw() <= addr.raw());
        TEST_ASSERT(!((addr.raw() - bufVirAddr.raw()) % allocSize));
        for (u64 j(0); j < numAllocs; ++j) {
            // FIXME: Once again, comparison operators for addresses.
            TEST_ASSERT(i == j || addr.raw() != allocations[j].raw());
        }
    }

    // Allocations should be continuous.
    for (u64 i(1); i < numAllocs; ++i) {
        u64 const prev(allocations[i - 1].raw());
        u64 const curr(allocations[i].raw());
        TEST_ASSERT(absdiff(prev, curr) == allocSize);
    }

    // Now free the even allocations.
    for (u64 i(0); i < numAllocs; i += 2) {
        freeList.free(allocations[i], allocSize);
    }

    // Sanity check: The free list should contain two nodes of size allocSize
    // each.
    EmbeddedFreeList::Node const * const first(freeList.m_head);
    TEST_ASSERT(!!first);
    TEST_ASSERT(first->size == allocSize);
    EmbeddedFreeList::Node const * const next(first->next);
    TEST_ASSERT(!!next);
    TEST_ASSERT(next->size == allocSize);

    // Now free the odd allocations.
    for (u64 i(1); i < numAllocs; i += 2) {
        freeList.free(allocations[i], allocSize);
    }

    // The free list should only have one node of size bufSize.
    TEST_ASSERT(!!freeList.m_head);
    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    return SelfTests::TestResult::Success;
}

// Run the frame allocation tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, earlyAllocatorTest);
    RUN_TEST(runner, embeddedFreeListNodeTest);
    RUN_TEST(runner, embeddedFreeListNodeOverlapTest);
    RUN_TEST(runner, embeddedFreeListNodeAdjacentWithTest);
    RUN_TEST(runner, embeddedFreeListInsertTest);
    RUN_TEST(runner, embeddedFreeListAllocFreeTest);
}
}