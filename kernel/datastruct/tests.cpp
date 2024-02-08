// Data structure tests.
#include <datastruct/datastruct.hpp>
#include <datastruct/freelist.hpp>
#include <selftests/macros.hpp>
#include "./vectortests.hpp"

namespace DataStruct {

// Test constructing an EmbeddedFreeList::Node from a virtual address and a
// size.
SelfTests::TestResult embeddedFreeListNodeTest() {
    u8 buf[64] = {0};
    EmbeddedFreeList::Node const * const node(
        EmbeddedFreeList::Node::fromVirAddr(buf, 64));
    TEST_ASSERT(node->base() == buf);
    TEST_ASSERT(node->end() == buf + 64 - 1);
    TEST_ASSERT(node->size == 64);
    return SelfTests::TestResult::Success;
}

// Check the overlapsWith function.
SelfTests::TestResult embeddedFreeListNodeOverlapTest() {
    u8 buf[64] = {0};
    EmbeddedFreeList::Node const * const node1(
        EmbeddedFreeList::Node::fromVirAddr(buf, 32));
    EmbeddedFreeList::Node const * const node2(
        EmbeddedFreeList::Node::fromVirAddr(buf + 32, 32));
    TEST_ASSERT(node1->overlapsWith(*node1));
    TEST_ASSERT(node2->overlapsWith(*node2));
    TEST_ASSERT(!node1->overlapsWith(*node2));
    TEST_ASSERT(!node2->overlapsWith(*node1));
    EmbeddedFreeList::Node const * const node3(
        EmbeddedFreeList::Node::fromVirAddr(buf + 31, 32));
    TEST_ASSERT(node1->overlapsWith(*node3));
    TEST_ASSERT(node3->overlapsWith(*node1));
    TEST_ASSERT(node2->overlapsWith(*node3));
    TEST_ASSERT(node3->overlapsWith(*node2));
    EmbeddedFreeList::Node const * const node4(
        EmbeddedFreeList::Node::fromVirAddr(buf + 8, 16));
    TEST_ASSERT(node1->overlapsWith(*node4));
    TEST_ASSERT(node4->overlapsWith(*node1));
    return SelfTests::TestResult::Success;
}

// Check the adjacentWith function.
SelfTests::TestResult embeddedFreeListNodeAdjacentWithTest() {
    u8 buf[64] = {0};
    // Positive tests.
    EmbeddedFreeList::Node const * const node1(
        EmbeddedFreeList::Node::fromVirAddr(buf, 16));
    EmbeddedFreeList::Node const * const node2(
        EmbeddedFreeList::Node::fromVirAddr(buf + 16, 16));
    TEST_ASSERT(node1->adjacentWith(*node2));
    TEST_ASSERT(node2->adjacentWith(*node1));
    // Negative tests.
    EmbeddedFreeList::Node const * const node3(
        EmbeddedFreeList::Node::fromVirAddr(buf + 16, 16));
    EmbeddedFreeList::Node const * const node4(
        EmbeddedFreeList::Node::fromVirAddr(buf, 15));
    EmbeddedFreeList::Node const * const node5(
        EmbeddedFreeList::Node::fromVirAddr(buf + 33, 16));
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
        freeList.insert(buf + nodeSize * i, nodeSize);
    }

    EmbeddedFreeList::Node* const first(freeList.m_head);
    TEST_ASSERT(first->size == nodeSize);
    TEST_ASSERT(!!first->next);
    EmbeddedFreeList::Node* const next(first->next);
    TEST_ASSERT(next->size == nodeSize);
    TEST_ASSERT(!next->next);

    // Insert odd nodes.
    for (u64 i(1); i < numNodes; i += 2) {
        freeList.insert(buf + nodeSize * i, nodeSize);
    }

    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    return SelfTests::TestResult::Success;
}

// Somewhat end-to-end test where we build a freelist and call alloc() and
// free() on it.
SelfTests::TestResult embeddedFreeListAllocFreeTest() {
    u64 const bufSize(256);
    // The buffer is initialized with non-zero bytes so that we can test that
    // the alloc() function does zero the allocated memory.
    u8 buf[bufSize] = {0xff};
    EmbeddedFreeList freeList;

    // Initialize the free list with the full buffer as free.
    freeList.insert(buf, bufSize);
    // Sanity check.
    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Make multiple allocations so that we allocate the entire buffer.
    u64 const numAllocs(4);
    ASSERT(!(bufSize % numAllocs));
    u64 const allocSize(bufSize / numAllocs);
    // Assumed by test.
    ASSERT(allocSize >= EmbeddedFreeList::MinAllocSize);

    VirAddr allocations[4];
    for (u64 i(0); i < numAllocs; ++i) {
        Res<VirAddr> const res(freeList.alloc(allocSize));
        TEST_ASSERT(res.ok());
        VirAddr const& allocAddr(res.value());
        allocations[i] = allocAddr;
        // Check that the memory has been zeroed.
        for (u64 j(0); j < allocSize; ++j) {
            TEST_ASSERT(!allocAddr.ptr<u8>()[j]);
        }
    }

    // Sanity check: The head of the EmbeddedFreeList should be nullptr.
    TEST_ASSERT(!freeList.m_head);

    // Trying to allocate once more is expected to fail.
    Res<VirAddr> const expFail(freeList.alloc(allocSize));
    TEST_ASSERT(!expFail.ok());
    TEST_ASSERT(expFail.error() == Error::OutOfPhysicalMemory);

    // Check that no two calls to alloc() returned the same address and that
    // each address' offset from the buffer is a multiple of the allocation
    // size.
    VirAddr const bufVirAddr(buf);
    for (u64 i(0); i < numAllocs; ++i) {
        VirAddr const& addr(allocations[i]);
        TEST_ASSERT(bufVirAddr <= addr);
        TEST_ASSERT(!((addr - bufVirAddr) % allocSize));
        for (u64 j(0); j < numAllocs; ++j) {
            TEST_ASSERT(i == j || addr != allocations[j]);
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

    // Try to allocate bufSize at once.
    Res<VirAddr> const allBuf(freeList.alloc(bufSize));
    TEST_ASSERT(allBuf.ok());
    TEST_ASSERT(!freeList.m_head);

    return SelfTests::TestResult::Success;
}

// Test the behaviour of the free list when requesting allocations that are less
// than the MinAllocSize.
SelfTests::TestResult embeddedFreeListAllocMinSizeTest() {
    // The buffer to allocate from can hold 2 entire MinAllocSize allocations
    // with some leftover bytes.
    u64 const bufSize(EmbeddedFreeList::MinAllocSize * 3 - 1);
    u8 buf[bufSize];
    EmbeddedFreeList freeList;

    // Initialize the free list with the full buffer as free.
    freeList.insert(buf, bufSize);
    // Sanity check.
    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Try to allocate a single byte. This should be successful and the
    // freelist's head should have "moved" by MinAllocSize.
    Res<VirAddr> const singleByteAlloc1(freeList.alloc(1));
    TEST_ASSERT(!!singleByteAlloc1);
    TEST_ASSERT(!!freeList.m_head);
    TEST_ASSERT(freeList.m_head->size ==
                bufSize - EmbeddedFreeList::MinAllocSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Trying to allocate a single byte again is expected to fail due to the
    // fact that allocating MinAllocSize bytes would not leave enough space in
    // the buffer to store the remaining Node.
    Res<VirAddr> const singleByteAlloc2(freeList.alloc(1));
    TEST_ASSERT(!singleByteAlloc2);
    // The head should not have changed.
    TEST_ASSERT(!!freeList.m_head);
    TEST_ASSERT(freeList.m_head->size ==
                bufSize - EmbeddedFreeList::MinAllocSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Allocating all the remaining bytes should be possible.
    u64 const lastAllocSize(freeList.m_head->size);
    Res<VirAddr> const lastAlloc(freeList.alloc(lastAllocSize));
    TEST_ASSERT(!!lastAlloc);
    TEST_ASSERT(!freeList.m_head);

    // Free the 1 byte allocation. Since it was smaller than the MinAllocSize
    // the free() actually frees MinAllocSize bytes.
    freeList.free(singleByteAlloc1.value(), 1);
    TEST_ASSERT(!!freeList.m_head);
    TEST_ASSERT(freeList.m_head->size == EmbeddedFreeList::MinAllocSize);
    TEST_ASSERT(!freeList.m_head->next);

    // Free the other allocation, the buffer is fully free at this point.
    freeList.free(lastAlloc.value(), lastAllocSize);
    TEST_ASSERT(!!freeList.m_head);
    TEST_ASSERT(freeList.m_head->size == bufSize);
    TEST_ASSERT(!freeList.m_head->next);

    return SelfTests::TestResult::Success;
}

void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, embeddedFreeListNodeTest);
    RUN_TEST(runner, embeddedFreeListNodeOverlapTest);
    RUN_TEST(runner, embeddedFreeListNodeAdjacentWithTest);
    RUN_TEST(runner, embeddedFreeListInsertTest);
    RUN_TEST(runner, embeddedFreeListAllocFreeTest);
    RUN_TEST(runner, embeddedFreeListAllocMinSizeTest);
    RUN_TEST(runner, vectorDefaultConstructionTest);
    RUN_TEST(runner, vectorConstructorSizeDefaultValueTest);
    RUN_TEST(runner, vectorConstructorSizeWithValueTest);
    RUN_TEST(runner, vectorDestructorTest);
    RUN_TEST(runner, vectorAccessorTest);
    RUN_TEST(runner, vectorClearTest);
    RUN_TEST(runner, vectorPushBackTest);
    RUN_TEST(runner, vectorPopBackTest);
    RUN_TEST(runner, vectorInsertFrontTest);
    RUN_TEST(runner, vectorInsertMiddleTest);
    RUN_TEST(runner, vectorEraseTest);
    RUN_TEST(runner, vectorIteratorTest);
    RUN_TEST(runner, vectorCopyTest);
    RUN_TEST(runner, vectorAssignTest);
    RUN_TEST(runner, vectorComparisonTest);
}
}
