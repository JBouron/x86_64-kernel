// Implementation of logging functions.

#pragma once

#include <util/util.hpp>
#include <util/error.hpp>
#include <util/addr.hpp>
#include <util/cstring.hpp>
#include <util/subrange.hpp>
#include <datastruct/vector.hpp>

namespace Logging {

// Helper class to print log message. There is typically a singleton Logger
// instance for the entire kernel.
class Logger {
public:
    // Color value for the output of the logger. There are only 4 colors, one
    // for each level of logging. OutputDev implementation are free to choose
    // what actual colors those values are mapping to.
    enum class Color {
        Info,
        Warn,
        Crit,
        Debug,
    };

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

        // Set the output color of the output dev. Some devices may choose to
        // ignore such calls.
        // @param color: The color.
        virtual void setColor(Color const color) = 0;
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
        u64 const len(Util::strlen(fmt));
        for (u64 i(0); i < len; ++i) {
            // Check if this is the start of a substitution, we are looking for
            // either "{}" or "{X}" where X is any char.
            char const curr(fmt[i]);
            bool const isSubst(fmt[i] == '{' && (
                ((i+1) < len && fmt[i+1] == '}') ||
                ((i+2) < len && fmt[i+2] == '}')));

            if (isSubst) {
                // There is a valid substitution, get the formatting option.
                char const next(fmt[i+1]);
                char const opt((next != '}') ? next : '\0');
                printValue(val, opt);
                u64 const offset(!!opt ? 3 : 2);
                // Recurse to print the next value. Note the +offset to skip the
                // formatting option and the closing bracket.
                printf(fmt + i + offset, o...);
                return;
            } else {
                m_dev.printChar(curr);
            }
        }
    }

    // Set the color of the logger's output.
    // @param color: The color to set.
    void setColor(Color const color);

private:
    // The underlying output device.
    OutputDev& m_dev;

    // Formatting option when printing a value. Such options are passed in
    // formatted strings within "{}".
    using FmtOption = char;

    // All overloads of printValue(T).
    void printValue(u64 const& val, FmtOption const& fmtOption);
    void printValue(u32 const& val, FmtOption const& fmtOption);
    void printValue(u16 const& val, FmtOption const& fmtOption);
    void printValue(u8 const& val, FmtOption const& fmtOption);
    void printValue(i64 const& val, FmtOption const& fmtOption);
    void printValue(int const& val, FmtOption const& fmtOption);
    void printValue(i16 const& val, FmtOption const& fmtOption);
    void printValue(i8 const& val, FmtOption const& fmtOption);
    void printValue(bool const& val, FmtOption const& fmtOption);
    void printValue(Error const& val, FmtOption const& fmtOption);
    void printValue(VirAddr const& val, FmtOption const& fmtOption);
    void printValue(PhyAddr const& val, FmtOption const& fmtOption);
    void printValue(char const* const& val, FmtOption const& fmtOption);

    // Print a const pointer of T in the output device.
    // @param ptr: the pointer to output to the device.
    // @param fmtOption: Ignored.
    template<typename T>
    void printValue(T const* ptr,
                    __attribute__((unused)) FmtOption const& fmtOption) {
        printNoNewLine("v:");
        printValue(reinterpret_cast<u64>(ptr), 'x');
    }

    // Print a pointer of T in the output device.
    // @param ptr: the pointer to output to the device.
    // @param fmtOption: Ignored.
    template<typename T>
    void printValue(T* ptr,
                    __attribute__((unused)) FmtOption const& fmtOption) {
        printNoNewLine("v:");
        printValue(reinterpret_cast<u64>(ptr), 'x');
    }

    // Print a SubRange type.
    template<typename Impl,
             u64 Min,
             u64 Max,
             u64 Default>
    void printValue(SubRange<Impl, Min, Max, Default> const& val,
                    FmtOption const& fmtOption) {
        printValue(val.raw(), fmtOption);
    }

    template<typename T>
    void printValue(Vector<T> const& vec, FmtOption const& fmtOption) {
        printNoNewLine("[");
        u64 const size(vec.size());
        for (u64 i(0); i < size; ++i) {
            printValue(vec[i], fmtOption);
            if (i < size - 1) {
                printNoNewLine(", ");
            }
        }
        printNoNewLine("]");
    }
};

}
