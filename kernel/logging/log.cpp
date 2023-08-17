// Util making logging easy.

#include <logging/log.hpp>
#include <logging/vga.hpp>
#include <logging/serial.hpp>

namespace Log {

// Return the global Logger singleton instance.
Logging::Logger& loggerInstance() {
    // This is where the global Logger instance is created.
#ifdef OUTPUT_VGA
    // FIXME: Eventually we will need to be able to set those from the Makefile,
    // for now leave it hardcoded.
    static Logging::VgaOutputDev outDev;
#else
    // Default to the serial console.
    static Logging::SerialOutputDev outDev(
        Logging::SerialOutputDev::ComPort::COM1);
#endif
    static Logging::Logger globalLogger(outDev);
    return globalLogger;
}

}
