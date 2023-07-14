// ASSERT macro definition.

#pragma once
#include <util/panic.hpp>

// Assert on a condition. If the condition is not satisfied a panic occurs.
// @param cond: The condition to assert on.
#define ASSERT(cond)                            \
    do {                                        \
        if (!(cond)) {                          \
            PANIC("Assert failed: " #cond);     \
        }                                       \
    } while (0)
