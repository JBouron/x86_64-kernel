// Some code common between timer implementations.
#pragma once
#include <util/subrange.hpp>
#include <selftests/selftests.hpp>

namespace Timer {
// Type for a frequency in Hz.
class Freq : public SubRange<Freq, 1, ~0ULL> {};

// Type for a value representing a duration.
class Duration {
public:
    // Create a Duration from microseconds.
    // @param us: The number of microseconds for the Duration value.
    // @return: A Duration value of `us` microseconds.
    static Duration MicroSecs(u64 const us);

    // Create a Duration from milliseconds.
    // @param ms: The number of milliseconds for the Duration value.
    // @return: A Duration value of `ms` milliseconds.
    static Duration MilliSecs(u64 const ms);

    // Create a Duration from seconds.
    // @param s: The number of seconds for the Duration value.
    // @return: A Duration value of `s` seconds.
    static Duration Secs(u64 const s);

    // Get the value of this duration.
    // @return: The duration in microseconds.
    u64 microSecs() const;

private:
    // Constructor. Only meant to be called from the static methods.
    // @param us: The number of microseconds for this Duration.
    Duration(u64 const us);

    // The value of this Duration in microseconds. All Durations are stored in
    // microseconds unit.
    u64 m_us;
};

// Run Timers tests.
void Test(SelfTests::TestRunner& runner);
}
