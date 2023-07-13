// Util making logging easy.

#include <logging/log.hpp>
#include <logging/vga.hpp>

namespace Log {

// Return the global Logger singleton instance.
Logging::Logger& loggerInstance() {
    // This is where the global Logger instance is created. For now we use the
    // VGA output dev, eventually we should be able to output to serial.
    static Logging::VgaOutputDev vgaOutputDev;
    static Logging::Logger globalLogger(vgaOutputDev);
    return globalLogger;
}

}
