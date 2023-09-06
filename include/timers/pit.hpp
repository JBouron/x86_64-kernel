// Interface to interact with the Programmable Interval Timer (PIT).
#pragma once
#include <timers/timers.hpp>
#include <interrupts/interrupts.hpp>
#include <selftests/selftests.hpp>

namespace Timer::Pit {

// The base frequency of the PIT in Hz.
static constexpr u64 BaseFrequency = 1193182;
 
// PIT historically uses IRQ #0.
// FIXME: Make SubRange constexpr.
static const Interrupts::Irq Irq = Interrupts::Irq(0);

// Map the PIT interrupts to a particular vector.
// @param vector: The vector to map PIT IRQs to.
void mapToVector(Interrupts::Vector const vector);

// Disable the PIT. Note that since there is no way to really disable the PIT
// this function simply configures the I/O APIC to mask the IRQ associated with
// the PIT.
void disable();

// Configure the PIT to fire an interrupt at the given frequency.
// @param freq: The frequency to configure the PIT to.
void setFrequency(Freq const freq);
}
