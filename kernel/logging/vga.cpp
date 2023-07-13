// Implementation of the VGA Logging::Logger::OutputDev.

#include <logging/vga.hpp>

namespace Logging {

// Create an instance of VgaOutputDev, using the default VGA text buffer.
// There is not much to do here, the bootloader already ID mapped the VGA buffer
// so we can just use the physical address, no mapping required. Clear the
// buffer for good measure.
VgaOutputDev::VgaOutputDev() : m_fgColor(15), m_cursorPos(0) {
    clear();
}

// Print a char in the output device.
// @param c: The character to be printed.
void VgaOutputDev::printChar(char const c) {
    u16 * const vgaBuffer(reinterpret_cast<u16*>(VgaBufferOffset));
    u16 * const writePtr(vgaBuffer + m_cursorPos);
    u16 const colorAttr(m_fgColor << 8);
    *writePtr = colorAttr | (u16)c;

    m_cursorPos ++;
    maybeScrollUpOneLine();
}

// Go to the next line.
void VgaOutputDev::newLine() {
    m_cursorPos += VgaBufferCols - (m_cursorPos % VgaBufferCols);
    maybeScrollUpOneLine();
}

// Clear the output device.
void VgaOutputDev::clear() {
    u16 * const vgaBuffer(reinterpret_cast<u16*>(VgaBufferOffset));
    u16 * writePtr(vgaBuffer);
    while (writePtr < vgaBuffer + VgaBufferRows * VgaBufferCols) {
        *(writePtr++) = 0;
    }
    // Reset the cursor to top-left of the buffer.
    m_cursorPos = 0;
}

void VgaOutputDev::maybeScrollUpOneLine() {
    if (m_cursorPos < VgaBufferRows * VgaBufferCols) {
        // Cursor is still within the bounds of the buffer, nothing to to here.
        return;
    }
    u16 * const vgaBuffer(reinterpret_cast<u16*>(VgaBufferOffset));
    u16 const * readPtr(vgaBuffer + VgaBufferCols);
    u16 * writePtr(vgaBuffer);
    while (readPtr < vgaBuffer + VgaBufferRows * VgaBufferCols) {
        *(writePtr++) = *(readPtr++);
    }

    // Clear the last line as it still contains its content from prior the
    // scroll.
    // FIXME: Assert writePtr == vgaBuffer + VgaBufferRows-1* VgaBufferCols
    while (writePtr < vgaBuffer + VgaBufferRows * VgaBufferCols) {
        *(writePtr++) = 0;
    }

    // Update the cursor to be on the last line of the buffer.
    m_cursorPos -= VgaBufferCols;
}
}
