// Functions to interact with and configure the LAPIC timer.
#pragma once
#include <timers/timers.hpp>
#include <interrupts/lapic.hpp>

namespace Timer::LapicTimer {

// Initialize the LAPIC timer. The timer is configured to use the given
// frequency and uses vector Interrupts::VectorMap::LapicTimerVector. The timer
// is _not_ started by calling this function.
// @param freq: The frequency to set the timer to.
void init(Freq const freq);

// Start the LAPIC timer. Must be called after init().
void start();

// Stop the LAPIC timer.
void stop();
}
