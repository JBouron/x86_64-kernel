#include <util/assert.hpp>
#include <logging/log.hpp>

// Print the condition that failed, the location of the assertion and stop the
// and halt the cpu forever. THIS DOES NOT RETURN.
void _raiseAssertFailure(char const * const condition,
                         char const * const fileName,
                         u64 const lineNumber,
                         char const * const funcName) {
    Log::crit("=============== ASSERT FAILURE ================");
    Log::crit("Location: {}:{}", fileName, lineNumber);
    Log::crit("Function: {}", funcName);
    // Using fmtWithPrefix and manually inserting the prefix is a bit hacky
    // here. Oh well...
    Log::fmtWithPrefixAndColor(Logging::Logger::Color::Crit,
                               "[CRIT] Condition: ",
                               condition);

    // Halt the CPU forever.
    while (true) {
        asm("cli");
        asm("hlt");
    }
}
