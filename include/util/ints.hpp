// Fixed-width signed and unsigned ints.

#pragma once
#include <stdint.h>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// Compute the min of two value, using operator<().
template<typename T>
inline T min(T const& a, T const& b) {
    return (a < b) ? a : b;
}

// Compute the max of two value, using operator>().
template<typename T>
inline T max(T const& a, T const& b) {
    return (a > b) ? a : b;
}

// Compute the difference between two unsigned values.
template<typename T>
inline T absdiff(T const& a, T const& b) {
    return (a > b) ? (a - b) : (b - a);
}
