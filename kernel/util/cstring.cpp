// Util functions to manipulate C-strings.

#include <util/cstring.hpp>

namespace Util {

// Compute the length of a string.
// @param str: The string.
// @return: The number of characters in `str`.
u64 strlen(char const * const str) {
    u64 len(0);
    char const * ptr(str);
    while (!!*(ptr++)) len++;
    return len;
}

// Compare two strings and indicates if they are equals.
// @param str1: The first operand.
// @param str2: The first operand.
// @return: true if the two strings have the same length and the same content,
// false otherwise.
bool streq(char const * const str1, char const * const str2) {
    char const * ptr1(str1);
    char const * ptr2(str2);
    while (!!*ptr1 && !!*ptr2 && *(ptr1++) == *(ptr2++));
    return !*ptr1 && !*ptr2;
}

// Zero a memory buffer.
// @param ptr: Memory buffer to zero.
// @param size: Size of the buffer in bytes.
void memzero(void * const ptr, u64 const size) {
    u8 * const wPtr(reinterpret_cast<u8*>(ptr));
    for (u64 i(0); i < size; ++i) {
        wPtr[i] = 0;
    }
}
}
