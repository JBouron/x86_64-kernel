// Functions to interact with and configure the LAPIC timer.
#pragma once
#include <timers/timers.hpp>

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

// Delay the current thread/cpu for the given duration. This is NOT a sleep, the
// core is simply busy-waiting for the entire duration. The interrupt flag is
// untouched during the delay.
// This function uses the LAPIC timer to perform the delay, hence cannot be
// used if the timer is already running!
// @param duration: The amount of time to delay the thread/core for.
void delay(Duration const duration);
}
