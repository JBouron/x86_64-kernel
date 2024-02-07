// Tests for the heap allocation functions.
#include "heapallocator.hpp"
#include <selftests/macros.hpp>

namespace HeapAlloc {

static constexpr u64 heapAllocatorTestNumFrames = 4;

// Frames allocated during the heapAllocatorTest.
static Frame heapAllocatorTestAllocatedFrames[heapAllocatorTestNumFrames];

// Index of the next allocation in heapAllocatorTestFrameAllocator.
static u64 heapAllocatorTestFrameAllocatorIndex = 0;

// Frame allocator for the heapAllocatorTest.
Res<Frame> heapAllocatorTestFrameAllocator() {
    u64 const idx(heapAllocatorTestFrameAllocatorIndex);
    ASSERT(idx < heapAllocatorTestNumFrames);
    heapAllocatorTestFrameAllocatorIndex++;
    return heapAllocatorTestAllocatedFrames[idx];    
}

SelfTests::TestResult heapAllocatorTest() {
    // Initialize the mock frame allocator.
    heapAllocatorTestFrameAllocatorIndex = 0;
    for (u64 i(0); i < heapAllocatorTestNumFrames; ++i) {
        Res<Frame> const alloc(FrameAlloc::alloc());
        heapAllocatorTestAllocatedFrames[i] = alloc.value();
    }

    // Instantiate the heap allocator to test.
    // Use an arbitrary start heap address.
    VirAddr const heapStart(0xdeadbeef000);
    u64 const maxHeapSize(heapAllocatorTestNumFrames * PAGE_SIZE);
    HeapAllocator allocator(heapStart,
                            maxHeapSize,
                            heapAllocatorTestFrameAllocator);

    // Test case #1: free() followed by alloc() should return the same address.
    Res<void*> const alloc1(allocator.alloc(10));
    TEST_ASSERT(alloc1.ok());
    Res<void*> const alloc2(allocator.alloc(10));
    TEST_ASSERT(alloc2.ok());
    // FIXME: Add comparison operators for Res<T>.
    TEST_ASSERT(*alloc2 != *alloc1);
    TEST_ASSERT(
        absdiff(reinterpret_cast<u64>(*alloc2), reinterpret_cast<u64>(*alloc1))
        == 10 + sizeof(HeapAllocator::Metadata));
    allocator.free(*alloc1);
    // Realloc 10 bytes we should get the same address as alloc1.
    Res<void*> const alloc3(allocator.alloc(10));
    TEST_ASSERT(alloc3.ok());
    TEST_ASSERT(*alloc3 == *alloc1);
    allocator.free(*alloc3);
    allocator.free(*alloc2);

    // Test case #2: Allocate more than PAGE_SIZE at once.
    Res<void*> const bigAlloc(allocator.alloc(PAGE_SIZE + 1));
    TEST_ASSERT(bigAlloc.ok());
    allocator.free(*bigAlloc);

    // Test case #3: Heap does not grow above limit.
    // Try to make an allocation of maxHeapSize bytes. Given that there is the
    // Metadata block this allocation cannot fit in the heap without bumping the
    // limit.
    Res<void*> const hugeAlloc(allocator.alloc(maxHeapSize));
    Log::info("^^^^ The error above is expected, part of testing ^^^^");
    TEST_ASSERT(!hugeAlloc.ok());
    TEST_ASSERT(hugeAlloc.error() == Error::MaxHeapSizeReached);
    // Double check that an allocation of maxHeapSize - sizeof(Metadata) is
    // allowed.
    u64 const maxAllocSize(maxHeapSize - sizeof(HeapAllocator::Metadata));
    Res<void*> const barelyFittingAlloc(allocator.alloc(maxAllocSize));
    TEST_ASSERT(barelyFittingAlloc.ok());
    allocator.free(*barelyFittingAlloc);

    // Test case #4: Allocations of 0 bytes.
    Res<void*> const zeroSizeAlloc1(allocator.alloc(0));
    Res<void*> const zeroSizeAlloc2(allocator.alloc(0));
    TEST_ASSERT(zeroSizeAlloc1.ok());
    TEST_ASSERT(zeroSizeAlloc2.ok());
    // The allocated pointers should only be separated by their metadata blocks.
    TEST_ASSERT(
        absdiff(reinterpret_cast<u64>(*zeroSizeAlloc1),
                reinterpret_cast<u64>(*zeroSizeAlloc2)) ==
        sizeof(HeapAllocator::Metadata));
    allocator.free(*zeroSizeAlloc1);
    allocator.free(*zeroSizeAlloc2);

    // Free all the allocated frames.
    for (u64 i(0); i < heapAllocatorTestNumFrames; ++i) {
        FrameAlloc::free(heapAllocatorTestAllocatedFrames[i]);
    }
    return SelfTests::TestResult::Success;
}

// Run heap allocation tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, heapAllocatorTest);
}
}
