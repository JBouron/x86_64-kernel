// Stub for the kernel C++ entry point. This is dead simple for now as we just
// want to exercise loading the kernel from the bootloader.

#include <stdint.h>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

static char const * const str = "Hello world from 64-bit kernel.cpp!!";

extern "C" void kernelEntry(void) {
    // Directly write into the VGA buffer. Append a line at the very bottom of
    // the buffer.
    u16 * const vgaBufferStart(reinterpret_cast<u16*>(0xB8000));
    u64 const vgaBufferCols(80);
    u64 const vgaBufferRows(25);

    u16 * writePtr(vgaBufferStart + (vgaBufferRows - 1) * vgaBufferCols);
    char const * strPtr(str);
    while (!!*strPtr) {
        u16 const bgColor(10);
        u16 const fgColor(0);
        u16 const colorAttr(((bgColor << 4) | fgColor) << 8);
        u16 const vgaChar(colorAttr | *strPtr);
        *writePtr = vgaChar;
        writePtr++;
        strPtr++;
    }

    while (true) {
        asm("cli");
        asm("hlt");
    }
}
