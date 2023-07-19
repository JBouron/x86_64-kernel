// Stub for the kernel C++ entry point. This is dead simple for now as we just
// want to exercise loading the kernel from the bootloader.

#include <util/util.hpp>
#include <logging/log.hpp>
#include <memory/segmentation.hpp>
#include <selftests/selftests.hpp>

extern "C" void kernelMain(void) {
    // Directly write into the VGA buffer. Append a line at the very bottom of
    // the buffer.
    Log::info("=== Kernel C++ Entry point ===");

    SelfTests::runSelfTests();

    Memory::Segmentation::Init();

    while (true) {
        asm("cli");
        asm("hlt");
    }
}
