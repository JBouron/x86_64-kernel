// Util header making logging easy. This is what code should include and use to
// print to log, there is no need to deal with the singleton Logger instance
// directly.
// A few levels of logging are available:
//  - DEBG: Debug messages.
//  - INFO: Informational messages.
//  - WARN: Warning messages.
//  - CRIT: Critical messages.
// Each level has its own helper function, e.g. Log::info(), Log::warn(), ...

#pragma once
#include <logging/logger.hpp>

namespace Log {

// Print a formatted string into the log. This is mostly used as a helper
// function for the helper functions below.
// @param color: The color to use for this string.
// @param prefix: The prefix to prepend to the log message.
// @param fmt: The formatted string.
// @param args: Arguments for the formatted string.
template<typename... T>
void fmtWithPrefixAndColor(Logging::Logger::Color const color,
                           char const * const prefix,
                           char const * const fmt,
                           T const&... args) {
    // Declared extern here so that code including this file don't have access
    // to loggerInstance().
    extern Logging::Logger& loggerInstance();
    Logging::Logger& logger(loggerInstance());
    logger.setColor(color);
    logger.printNoNewLine(prefix);
    logger.printf(fmt, args...);
}

// Output an info level log message.
// @param fmt: The formatted string.
// @param args: Arguments for the formatted string.
template<typename... T>
void info(char const * const fmt, T const&... args) {
    fmtWithPrefixAndColor(Logging::Logger::Color::Info,
                          "[INFO] ",
                          fmt,
                          args...);
}

// Output a warning level log message.
// @param fmt: The formatted string.
// @param args: Arguments for the formatted string.
template<typename... T>
void warn(char const * const fmt, T const&... args) {
    fmtWithPrefixAndColor(Logging::Logger::Color::Warn,
                          "[WARN] ",
                          fmt,
                          args...);
}

// Output a critical level log message.
// @param fmt: The formatted string.
// @param args: Arguments for the formatted string.
template<typename... T>
void crit(char const * const fmt, T const&... args) {
    fmtWithPrefixAndColor(Logging::Logger::Color::Crit,
                          "[CRIT] ",
                          fmt,
                          args...);
}

// Output a debug level log message.
// @param fmt: The formatted string.
// @param args: Arguments for the formatted string.
template<typename... T>
void debug(char const * const fmt, T const&... args) {
    fmtWithPrefixAndColor(Logging::Logger::Color::Debug,
                          "[DEBG] ",
                          fmt,
                          args...);
}

}
