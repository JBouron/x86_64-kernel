// PANIC macro definition.

#pragma once

#include <logging/log.hpp>

// Trigger a kernel panic with the given message (a formatted string). This
// prints the panic message and then halts the cpu forever. THIS DOES NOT
// RETURN.
// @param fmt: The format string.
// @param (variadic): The arguments for the format string.
#define PANIC(fmt, ...) \
    _panic(__FILE__,__LINE__,__PRETTY_FUNCTION__,fmt __VA_OPT__(,) __VA_ARGS__)

// Do not use directly, use PANIC() instead. Helper function for the PANIC
// macro, prints the panic message and halt the cpud forever.
// @param fileName: Name of the file in which the panic occured.
// @param lineNumber: Line number in the file `fileName` at which the panic
// occured.
// @param funcName: The name of the function in which the panic occured.
// @param fmt: Format string for the panic message.
// @param args: Arguments for the format string.
template<typename... T>
void _panic(char const * const fileName,
            u64 const lineNumber,
            char const * const funcName,
            char const * const fmt,
            T const&... args) {
    Log::crit("==================== PANIC ====================");
    Log::crit("  Location: {}:{} in {}", fileName, lineNumber, funcName);
    // Using fmtWithPrefix and manually inserting the prefix is a bit hacky
    // here. Oh well...
    Log::fmtWithPrefixAndColor(Logging::Logger::Color::Crit,
                               "[CRIT]   Reason: ",
                               fmt,
                               args...);

    // Halt the CPU forever.
    while (true) {
        asm("cli");
        asm("hlt");
    }
}

// Mark a section of code as unreachable. Triggers a PANIC if it is ever
// reached.
#define UNREACHABLE                         \
    do {                                    \
        PANIC("Reached unreachable code!"); \
        __builtin_unreachable();            \
    } while (0);
