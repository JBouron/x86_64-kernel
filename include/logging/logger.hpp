// Implementation of logging functions.

#pragma once

#include <util/util.hpp>

namespace Logging {

// Helper class to print log message. There is typically a singleton Logger
// instance for the entire kernel.
class Logger {
public:
    // The underlying device of a Logger instance, e.g. VGA, serial, ...
    // This class is meant to be derived by the class implementing the actual
    // output device.
    class OutputDev {
    public:
        // Print a char in the output device.
        // @param c: The character to be printed.
        virtual void printChar(char const c) = 0;

        // Go to the next line.
        virtual void newLine() = 0;

        // Clear the output device.
        virtual void clear() = 0;
    };

    // Create a new Logger instance using the given OutputDev as backend.
    // @param dev: The underlying OutputDev to be used.
    Logger(OutputDev& dev);

    // Clear the underlying output device.
    void clear();

    // Print a string into the log but do not append a new line at the end of
    // it. This is useful when prefixing log messages.
    // @param str: The string to print into the log.
    void printNoNewLine(char const * const str);

    // Print a string into the log. This is the base case of the variadic printf
    // method. A new line character is appended after the string.
    // @param str: The string to print into the log.
    void printf(char const * const str);

    // Print a formatted string into the log. Each "{}" is replaced with the
    // next value in the parameter list not yet printed.
    // @param fmt: The formatted string.
    // @param val...: Arguments for the formatted string.
    template<typename T, typename... Others>
    void printf(char const * const fmt, T const& val, Others const&... o) {
        for (char const * ptr(fmt); *ptr; ++ptr) {
            char const curr(*ptr);
            char const next(*(ptr + 1));
            if (curr == '{' && next == '}') {
                printValue(val);
                // Recurse to print the next value. Note the +2 to skip the
                // closing bracket.
                printf(ptr + 2, o...);
                return;
            } else {
                m_dev.printChar(curr);
            }
        }
    }

private:
    // The underlying output device.
    OutputDev& m_dev;

    // Print a value of type T in the output device.
    // @param val: the value to output to the device.
    template<typename T>
    void printValue(T const& val);
};

}
