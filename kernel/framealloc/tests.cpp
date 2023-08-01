// Test for FrameAlloc namespace.
#include <framealloc/framealloc.hpp>
#include <bootstruct.hpp>
#include "allocator.hpp"

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

// Run the frame allocation tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, earlyAllocatorTest);
}
}
