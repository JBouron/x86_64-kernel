// File containing all the interrupt vectors used in the kernel.
#pragma once
#include <interrupts/interrupts.hpp>

namespace Interrupts::VectorMap {
// Vector used by the Programmable Interval Timer (PIT) when computing the
// frequency of the LAPIC timer.
static const Vector PitVector = Vector(32);

// Vector used by the LAPIC timer.
static const Vector LapicTimerVector = Vector(33);

// Vector used by various tests. Since tests are ran serially this vector should
// always be available to use.
static const Vector TestVector = Vector(34);

// Vector used to notify a cpu that a new remote call has been enqueued in its
// remote call queue.
static const Vector RemoteCallVector = Vector(35);
}
