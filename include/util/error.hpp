// Error enum definition.
#pragma once

// Indicate an error condition. Mostly used by the Res<T> class.
enum class Error {
    // No more physical memory available.
    OutOfPhysicalMemory,

    // The heap cannot grow anymore due to the fact that it reached its maximum
    // size.
    MaxHeapSizeReached,

    // The ACPI parsing function was not able to file the RSDP.
    NoRsdpFound,

    // To be used for testing only.
    Test,
};

// Map an Error value to its string representation.
// @param value: The value to transform into a cstring.
// @return: The cstring representation of the value.
inline char const * errorToString(Error const value) {
#define CASE(value) case Error::value : return #value ;
    switch (value) {
        CASE(OutOfPhysicalMemory)
        CASE(MaxHeapSizeReached)
        CASE(NoRsdpFound)
        CASE(Test)
        // -Wall and -Werror make sure that all values of Error must appear
        // here.
    }
    __builtin_unreachable();
#undef CASE
}
