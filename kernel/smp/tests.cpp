// SMP tests.
#include <smp/smp.hpp>
#include <paging/paging.hpp>
#include <timers/lapictimer.hpp>

// Used by wakeApplicationProcessorTest.
// This code starts with a 4-bytes NOP-slides to real-mode code that replaces
// this slide with a DWORD of value 0xb1e2b007.
extern "C" void wakeApplicationProcessorTestBootCode();
extern "C" void wakeApplicationProcessorTestBootCodeEnd();

namespace Smp {

// Test waking-up an application processor.
SelfTests::TestResult wakeApplicationProcessorTest() {
    // FIXME: We don't really have a way to allocate a Frame that is under the
    // 1MiB limit, so for now we simply use a page that is used by the
    // bootloader, it won't mind.
    PhyAddr const bootFrame(0x8000);
    VirAddr const bootFrameVir(bootFrame.toVir());

    // Copy the bootstrap code.
    u64 const bootStrapCodeSize(
        reinterpret_cast<u64>(&wakeApplicationProcessorTestBootCodeEnd)
        - reinterpret_cast<u64>(&wakeApplicationProcessorTestBootCode));
    Util::memcpy(bootFrameVir.ptr<void>(),
                 reinterpret_cast<void*>(&wakeApplicationProcessorTestBootCode),
                 bootStrapCodeSize);

    // Sanity check: we should have a NOP-slide at the very beginning of the
    // bootstrap code.
    TEST_ASSERT(*bootFrameVir.ptr<u32>() == 0x90909090);
                    
    // Wake up cpu 1 and make it run the bootstrap code.
    wakeApplicationProcessor(Smp::Id(1), bootFrame);

    // Wait for cpu 1 to execute the boot code.
    TEST_WAIT_FOR(*bootFrameVir.ptr<u32>() == 0xb1e2b007, 1000);
    return SelfTests::TestResult::Success;
}

// Run SMP tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, wakeApplicationProcessorTest);
}
}
