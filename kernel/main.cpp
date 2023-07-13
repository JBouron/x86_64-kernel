// Stub for the kernel C++ entry point. This is dead simple for now as we just
// want to exercise loading the kernel from the bootloader.

#include <util/util.hpp>
#include <logging/log.hpp>

extern "C" void kernelMain(void) {
    // Directly write into the VGA buffer. Append a line at the very bottom of
    // the buffer.

    Log::info("=== Kernel C++ Entry point ===");
    Log::info("Hello Kernel World!");

    while (true) {
        asm("cli");
        asm("hlt");
    }
}
