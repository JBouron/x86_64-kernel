// C++ entry point of the kernel.

#include <util/util.hpp>
#include <logging/log.hpp>
#include <memory/segmentation.hpp>
#include <selftests/selftests.hpp>
#include <cpu/cpu.hpp>
#include <interrupts/interrupts.hpp>
#include <bootstruct.hpp>
#include <framealloc/framealloc.hpp>
#include <paging/paging.hpp>
#include <util/result.hpp>
#include <util/err.hpp>
#include <datastruct/datastruct.hpp>

static void runSelfTests() {
    Log::info("Running self-tests:");
    SelfTests::TestRunner runner;

    Cpu::Test(runner);
    Interrupts::Test(runner);
    Paging::Test(runner);
    FrameAlloc::Test(runner);
    Result::Test(runner);
    ErrType::Test(runner);
    DataStruct::Test(runner);

    runner.printSummary();
}

// Log the bootstruct's content.
// @param bootStruct: The bootstruct to dump into the logs.
static void dumpBootStruct(BootStruct const& bootStruct) {
    Log::info("Physical memory map:");
    for (u64 i(0); i < bootStruct.memoryMapSize; ++i) {
        BootStruct::MemMapEntry const& entry(bootStruct.memoryMap[i]);
        char const * const type(entry.isAvailable() ? "Available" : "Reserved");
        u64 const end(entry.base + entry.length);
        Log::info("  {x} - {x}  {}", entry.base, end, type);
    }

    Log::debug("Physical frame free-list:");
    BootStruct::PhyFrameFreeListNode const * curr(
        bootStruct.phyFrameFreeListHead);
    while (!!curr) {
        Log::debug("  {} frames starting @{x}", curr->numFrames, curr->base);
        curr = curr->next;
    }
}

// C++ entry point of the kernel. Called by the assembly entry point
// `kernelEntry` after calling all global constructors.
// @param bootStruct: Pointer to the BootStruct as given by the bootloader when
// jumping to the kernel's assembly entry point.
extern "C" void kernelMain(BootStruct const * const bootStruct) {
    Log::info("=== Kernel C++ Entry point ===");

    Log::debug("Bootstruct is @{x}", reinterpret_cast<u64>(bootStruct));
    dumpBootStruct(*bootStruct);

    Memory::Segmentation::Init();
    Interrupts::Init();
    FrameAlloc::Init(*bootStruct);
    Paging::Init(*bootStruct);
    // Now that paging and the direct map have been initialized, we can switch
    // to the proper frame allocator.
    FrameAlloc::directMapInitialized();

    runSelfTests();

    while (true) {
        asm("cli");
        asm("hlt");
    }
}
