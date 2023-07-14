// Util functions to manipulate C-strings.

#pragma once

#include <util/util.hpp>

namespace Util {

// Compute the length of a string.
// @param str: The string.
// @return: The number of characters in `str`.
u64 strlen(char const * const str);

}
