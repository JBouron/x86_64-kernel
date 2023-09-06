// Some code common between timer implementations.
#include <timers/timers.hpp>

namespace Timer {
// Create a Duration from microseconds.
// @param us: The number of microseconds for the Duration value.
// @return: A Duration value of `us` microseconds.
Duration Duration::MicroSecs(u64 const us) {
    return Duration(us);
}

// Create a Duration from milliseconds.
// @param ms: The number of milliseconds for the Duration value.
// @return: A Duration value of `ms` milliseconds.
Duration Duration::MilliSecs(u64 const ms) {
    return Duration(ms * 1000);
}

// Create a Duration from seconds.
// @param s: The number of seconds for the Duration value.
// @return: A Duration value of `s` seconds.
Duration Duration::Secs(u64 const s) {
    return Duration(s * 1000000);
}

// Get the value of this duration.
// @return: The duration in microseconds.
u64 Duration::microSecs() const {
    return m_us;
}

// Constructor. Only meant to be called from the static methods.
// @param us: The number of microseconds for this Duration.
Duration::Duration(u64 const us) : m_us(us) {}
}
