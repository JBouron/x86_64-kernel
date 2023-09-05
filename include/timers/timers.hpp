// Some code common between timer implementations.
#pragma once
#include <util/subrange.hpp>

namespace Timer {
// Type for a frequency in Hz.
class Freq : public SubRange<Freq, 1, ~0ULL> {};
}
