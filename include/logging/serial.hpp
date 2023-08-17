// Definition of the SerialOutputDev.
#pragma once
#include <logging/logger.hpp>
#include <cpu/cpu.hpp>

namespace Logging {

// Implementation of logging through the serial console.
class SerialOutputDev : public Logger::OutputDev {
public:
    // A COM port address. COM1 and COM2 are guaranteed to be at the specified
    // address. Other ports are less reliable. See
    // https://wiki.osdev.org/Serial_Ports#Port_Addresses.
    enum class ComPort : u16 {
        COM1 = 0x3F8,
        COM2 = 0x2F8,
        COM3 = 0x3E8,
        COM4 = 0x2E8,
        COM5 = 0x5F8,
        COM6 = 0x4F8,
        COM7 = 0x5E8,
        COM8 = 0x4E8,
    };

    // The baud rate used by the implementation.
    static u32 const BaudRate = 115200;

    // Create a serial output device using the given port.
    // @param port: The port to be used.
    SerialOutputDev(ComPort const port);

    // Print a char in the output device.
    // @param c: The character to be printed.
    virtual void printChar(char const c);

    // Go to the next line.
    virtual void newLine();

    // Clear the output device.
    virtual void clear();

    // Set the output color of the output dev. Some devices may choose to
    // ignore such calls.
    // @param color: The color.
    virtual void setColor(Logger::Color const color);

private:
    // The COM port to be used by this device.
    ComPort const m_port;

    // COM registers.
    enum class Register : u8 {
        Data = 0,
        InterruptEnable = 1,
        LineControl = 3,
        LineStatus = 5,
        Scratch = 7,

        // Special values for the Divisor registers as those require setting the
        // DLAB bit.
        DivisorLow = 254,
        DivisorHigh = 255,
    };

    // Compute the port needed to read/write a register.
    // @param reg: The register to get the port for.
    // @return: The port to be used in the I/O instruction to access this
    // register.
    // Note that the caller needs to be aware if accessing the register requires
    // setting the DLAB or not.
    Cpu::Port registerToPort(Register const reg) const;

    // Read the current value of a register.
    // @param reg: The register to read.
    // @return: The value of the register `reg`.
    u8 readRegister(Register const reg);

    // Write into a register.
    // @param reg: The register to write into.
    // @param value: The value to write into `reg`.
    void writeRegister(Register const reg, u8 const value);

    // Set the value of the Divisor Latch Access Bit (DLAB).
    // @param value: The value to set the DLAB bit to.
    void setDlab(bool const value);

    // Check if the controller is ready to send data.
    // @return: true if the controller can send data, false otherwise.
    bool canSendData();
};
}
