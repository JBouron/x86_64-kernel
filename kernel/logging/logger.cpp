#include <logging/logger.hpp>
#include <util/error.hpp>
#include <util/addr.hpp>

namespace Logging {

// Create a new Logger instance using the given OutputDev as backend.
// @param dev: The underlying OutputDev to be used.
Logger::Logger(OutputDev& dev) : m_dev(dev) {}

// Clear the underlying output device.
void Logger::clear() {
    m_dev.clear();
}

// Print a string into the log but do not append a new line at the end of
// it. This is useful when prefixing log messages.
// @param str: The string to print into the log.
void Logger::printNoNewLine(char const * const str) {
    for (char const * ptr(str); *ptr; ++ptr) {
        m_dev.printChar(*ptr);
    }
}

// Print a string into the log. This is the base case of the variadic printf
// method.
// @param str: The string to print into the log.
void Logger::printf(char const * const str) {
    printNoNewLine(str);
    m_dev.newLine();
}

// printValue<T> specialization for all currently supported types:

// Quick and dirty implementation of printing a u64 in decimal format.
template<>
void Logger::printValue<u64>(u64 const& val, FmtOption const& fmtOption) {
    if (fmtOption == 'x') {
        // Print in base 16 with leading zeroes.
        printNoNewLine("0x");
        u64 shift(60);
        u64 mask(0xfULL << shift);
        while (!!mask) {
            u8 const digit((val & mask) >> shift);
            if (digit < 10) {
                m_dev.printChar('0' + digit);
            } else {
                m_dev.printChar('a' + digit - 10);
            }
            mask >>= 4;
            shift -= 4;
        }
    } else {
        // Default behaviour is to print in base 10.
        if (!val) {
            // The code below assumes that val > 0.
            m_dev.printChar('0');
            return;
        }
        // The max u64 takes 20 digits to represent in base 10.
        static constexpr u64 maxDigits = 20;
        u8 digits[maxDigits] = {0};
        u64 power(1);
        for (u64 i(0); i < maxDigits; ++i) {
            digits[i] = (val / power) % 10;
            power *= 10;
        }
        bool outputZeros(false);
        for (u64 i(0); i < maxDigits; ++i) {
            u8 const d(digits[19 - i]);
            if (!outputZeros && !d) {
                continue;
            } else {
                outputZeros = true;
                m_dev.printChar('0' + d);
            }
        }
    }
}

// For the u8, u16 and u32 versions, use printValue<u64>
template<>
void Logger::printValue<u32>(u32 const& val, FmtOption const& fmtOption) {
    printValue<u64>(val, fmtOption);
}
template<>
void Logger::printValue<u16>(u16 const& val, FmtOption const& fmtOption) {
    printValue<u64>(val, fmtOption);
}
template<>
void Logger::printValue<u8>(u8 const& val, FmtOption const& fmtOption) {
    printValue<u64>(val, fmtOption);
}

// Quick and dirty implementation of printing a i64 in decimal format.
template<>
void Logger::printValue<i64>(i64 const& val, FmtOption const& fmtOption) {
    // Leverage printValue<u64> to print the absolute value of val, which
    // definitely fits in a u64.
    if (val < 0) {
        m_dev.printChar('-');
    }
    u64 const absVal((val < 0) ? u64(-val) : val);
    printValue(absVal, fmtOption);
}

// For the i8, i16 and i32 versions, use printValue<i64>
template<>
void Logger::printValue<i32>(i32 const& val, FmtOption const& fmtOption) {
    printValue<i64>(val, fmtOption);
}
template<>
void Logger::printValue<i16>(i16 const& val, FmtOption const& fmtOption) {
    printValue<i64>(val, fmtOption);
}
template<>
void Logger::printValue<i8>(i8 const& val, FmtOption const& fmtOption) {
    printValue<i64>(val, fmtOption);
}

template<>
void Logger::printValue<Error>(Error const& val,
    __attribute__((unused)) FmtOption const& fmtOption) {
    printNoNewLine(errorToString(val));
}

template<>
void Logger::printValue<VirAddr>(VirAddr const& val,
    __attribute__((unused)) FmtOption const& fmtOption) {
    printNoNewLine("v:");
    printValue<u64>(val.raw(), 'x');
}

template<>
void Logger::printValue<PhyAddr>(PhyAddr const& val,
    __attribute__((unused)) FmtOption const& fmtOption) {
    printNoNewLine("p:");
    printValue<u64>(val.raw(), 'x');
}

void Logger::setColor(Color const color) {
    m_dev.setColor(color);
}
}
