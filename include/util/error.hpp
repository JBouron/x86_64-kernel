// Error enum definition.
#pragma once

// Indicate an error condition. Mostly used by the Res<T> class.
enum class Error {
    // No more physical memory available.
    OutOfPhysicalMemory,

    // To be used for testing only.
    Test,
};
