// Util functions to manipulate C-strings.

#pragma once

#include <util/ints.hpp>

namespace Util {

// Compute the length of a string.
// @param str: The string.
// @return: The number of characters in `str`.
u64 strlen(char const * const str);

// Compare two strings and indicates if they are equals.
// @param str1: The first operand.
// @param str2: The first operand.
// @return: true if the two strings have the same length and the same content,
// false otherwise.
bool streq(char const * const str1, char const * const str2);

// FIXME: Do we really need to use void here?
// Zero a memory buffer.
// @param ptr: Memory buffer to zero.
// @param size: Size of the buffer in bytes.
void memzero(void * const ptr, u64 const size);

// FIXME: Do we really need to use void here?
// Copy a memory buffer into another
// @param dest: The destination buffer.
// @param src: The source buffer.
// @param size: The number of bytes to copy.
void memcpy(void * const dst, void const * const src, u64 const size);
}
