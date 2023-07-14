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

}
