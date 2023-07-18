// Declaration of the VGA Logging::Logger::OutputDev.

#pragma once

#include <logging/logger.hpp>

namespace Logging {

// Logger::OutputDev outputting to the VGA text buffer.
class VgaOutputDev : public Logger::OutputDev {
public:
    // Create an instance of VgaOutputDev, using the default VGA text buffer.
    // This clears the VGA text buffer.
    VgaOutputDev();

    // Print a char in the output device.
    // @param c: The character to be printed.
    virtual void printChar(char const c);

    // Go to the next line.
    virtual void newLine();

    // Clear the output device.
    virtual void clear();

    // Set the output color of the output dev.
    // @param color: The color.
    virtual void setColor(Logger::Color const color);

private:
    // The VGA text buffer is ID-mapped by the bootloader hence safe to use
    // here.
    static constexpr u64 VgaBufferOffset = 0xB8000;
    static constexpr u64 VgaBufferCols = 80;
    static constexpr u64 VgaBufferRows = 25;

    // The VGA color table.
    enum class Color : u16  {
        Black       = 0,
        Blue        = 1,
        Green       = 2,
        Cyan        = 3,
        Red         = 4,
        Purple      = 5,
        Brown       = 6,
        Gray        = 7,
        DarkGray    = 8,
        LightBlue   = 9,
        LightGreen  = 10,
        LightCyan   = 11,
        LightRed    = 12,
        LightPurple = 13,
        Yellow      = 14,
        White       = 15,
    };

    // Current foreground color.
    Color m_fgColor;

    // Linear index of the cursor in the VGA buffer.
    u64 m_cursorPos;

    // Scroll, if needed, the entire VGA text buffer up one line. The scroll
    // happens if the cursor is outside the bounds of the buffer.
    void maybeScrollUpOneLine();
};
}
