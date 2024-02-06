// ASSERT macro definition.
#pragma once
#include <util/ints.hpp>

// Raise a PANIC, printing the condition that failed.
void _raiseAssertFailure(char const * const condition,
                         char const * const fileName,
                         u64 const lineNumber,
                         char const * const funcName);

// Assert on a condition. If the condition is not satisfied a panic occurs.
// @param cond: The condition to assert on.
#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            _raiseAssertFailure(#cond,__FILE__,__LINE__,__PRETTY_FUNCTION__);  \
        }                                                                      \
    } while (0)
