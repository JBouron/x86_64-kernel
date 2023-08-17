// Definition of the SerialOutputDev.
#include <logging/serial.hpp>

namespace Logging {

static u32 const ClockFreq = 115200;

// Create a serial output device using the given port.
// @param port: The port to be used.
SerialOutputDev::SerialOutputDev(ComPort const port) : m_port(port) {
    // Initialize the serial port.

    // Set the divisor for the target baudrate. Make sure not to set it to zero.
    u16 const divisor(max(1u, ClockFreq / BaudRate));
    writeRegister(Register::DivisorLow, divisor & 0xf);
    writeRegister(Register::DivisorHigh, divisor >> 8);

    // Set up the LineControl register.
    u8 const lineControl(readRegister(Register::LineControl));
    // 8-bits, no parity and 1 stop bit.
    u8 const newLineControl((lineControl & ~0b111111) | 0x3);
    writeRegister(Register::LineControl, newLineControl);

    // Disable any interrupt.
    writeRegister(Register::InterruptEnable, 0x0);
}

// Print a char in the output device.
// @param c: The character to be printed.
void SerialOutputDev::printChar(char const c) {
    // New-lines must be preceeded by a carriage return.
    if (c == '\n') {
        printChar('\r');
    }

    // Wait for the transmitter to be ready.
    while (!canSendData());

    // Write the char.
    writeRegister(Register::Data, c);
}

// Go to the next line.
void SerialOutputDev::newLine() {
    printChar('\n');
}

// Clear the output device.
void SerialOutputDev::clear() {
    // Not supported with serial.
}

// Set the output color of the output dev. Some devices may choose to
// ignore such calls.
// @param color: The color.
void SerialOutputDev::setColor(Logger::Color const color) {
    // FIXME: Add support for color through bash escape sequences.
    (void)color;
}

// Compute the port needed to read/write a register.
// @param reg: The register to get the port for.
// @return: The port to be used in the I/O instruction to access this register.
// Note that the caller needs to be aware if accessing the register requires
// setting the DLAB or not.
Cpu::Port SerialOutputDev::registerToPort(Register const reg) const {
    Cpu::Port const base(static_cast<u16>(m_port));
    if (reg == Register::DivisorLow || reg == Register::DivisorHigh) {
        return base + static_cast<u8>(reg) - 254;
    } else {
        return base + static_cast<u8>(reg);
    }
}

// Read the current value of a register.
// @param reg: The register to read.
// @return: The value of the register `reg`.
u8 SerialOutputDev::readRegister(Register const reg) {
    bool const needToggleDlab(reg == Register::DivisorLow
                              || reg == Register::DivisorHigh);
    if (needToggleDlab) {
        setDlab(true);
    }

    Cpu::Port const port(registerToPort(reg));
    u8 const value(Cpu::inb(port));

    if (needToggleDlab) {
        setDlab(false);
    }
    return value;
}

// Write into a register.
// @param reg: The register to write into.
// @param value: The value to write into `reg`.
void SerialOutputDev::writeRegister(Register const reg, u8 const value) {
    bool const needToggleDlab(reg == Register::DivisorLow
                              || reg == Register::DivisorHigh);
    if (needToggleDlab) {
        setDlab(true);
    }

    Cpu::Port const port(registerToPort(reg));
    Cpu::outb(port, value);

    if (needToggleDlab) {
        setDlab(false);
    }
}

// Set the value of the Divisor Latch Access Bit (DLAB).
// @param value: The value to set the DLAB bit to.
void SerialOutputDev::setDlab(bool const value) {
    u8 const lineControlValue(readRegister(Register::LineControl));
    u8 const newValue((lineControlValue & 0x7f) | ((u8)value * 0x80));
    writeRegister(Register::LineControl, newValue);
}

// Check if the controller is ready to send data.
// @return: true if the controller can send data, false otherwise.
bool SerialOutputDev::canSendData() {
    return !!(readRegister(Register::LineStatus) & 0x20);
}
}
