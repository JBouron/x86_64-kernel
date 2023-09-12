// Test for FrameAlloc namespace.
#include <framealloc/framealloc.hpp>
#include <util/assert.hpp>
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

    TEST_ASSERT(allocator.alloc().value().addr() == 0x0);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x10000);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x20000);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x21000);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x30000);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x31000);
    TEST_ASSERT(allocator.alloc().value().addr() == 0x32000);

    // No more memory, the next allocation should fail.
    Res<Frame> const last(allocator.alloc());
    TEST_ASSERT(!last.ok());
    TEST_ASSERT(last.error() == Error::OutOfPhysicalMemory);

    return SelfTests::TestResult::Success;
}

// Test the EmbeddedFreeListAllocator.
SelfTests::TestResult embeddedFreeListAllocatorTest() {
    // Allocate a few physical frames that will be used by the frame allocator
    // being tested.
    u64 const numFrames(8);
    Frame frames[numFrames];
    for (u64 i(0); i < numFrames; ++i) {
        Res<Frame> const alloc(FrameAlloc::alloc());
        ASSERT(!!alloc);
        frames[i] = alloc.value();
    }

    // Build the frame allocator to be tested.
    EmbeddedFreeListAllocator frameAllocator;
    for (u64 i(0); i < numFrames; ++i) {
        VirAddr const frameVAddr(frames[i].addr().toVir());
        frameAllocator.insertFreeRegion(frameVAddr, 1);
    }

    // We repeat the following experiment twice.
    for (u64 run(0); run < 2; ++run) {
        // Allocate numFrames. All allocations are expected to be successful AND
        // return a Frame that is in the frames[] array.
        for (u64 i(0); i < numFrames; ++i) {
            Res<Frame> const allocRes(frameAllocator.alloc());
            Frame const& frame(allocRes.value());
            // Dirty nested loop but that will do the job.
            bool found(false);
            for (u64 j(0); j < numFrames; ++j) {
                if (frame == frames[j]) {
                    found = true;
                    break;
                }
            }
            // If this assert fails then the allocator returned a frame that was not
            // inserted when we built it. This prob means the allocator allocates
            // random frames.
            TEST_ASSERT(found);
        }

        // Trying to allocate one more frame should fail since there should not be
        // any more free frames.
        TEST_ASSERT(!frameAllocator.alloc().ok());

        // Free all even frames.
        for (u64 i(0); i < numFrames; i += 2) {
            frameAllocator.free(frames[i]);
        }

        // Free all odd frames.
        for (u64 i(1); i < numFrames; i += 2) {
            frameAllocator.free(frames[i]);
        }
        // The next run will make sure that we can still allocate numFrames
        // after freeing all of them
    }

    return SelfTests::TestResult::Success;
}

// Run the frame allocation tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, earlyAllocatorTest);
    RUN_TEST(runner, embeddedFreeListAllocatorTest);
}
}
