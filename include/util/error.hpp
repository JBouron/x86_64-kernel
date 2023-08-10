// Error enum definition.
#pragma once

// Indicate an error condition. Mostly used by the Res<T> class.
enum class Error {
    // No more physical memory available.
    OutOfPhysicalMemory,

    // The heap cannot grow anymore due to the fact that it reached its maximum
    // size.
    MaxHeapSizeReached,

    // To be used for testing only.
    Test,
};
