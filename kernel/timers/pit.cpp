// Interface to interact with the Programmable Interval Timer (PIT).
// We keep the interface and PIT code in general simple and short as the PIT is
// only needed to calibrate and compute the frequency of the LAPIC timer. Hence
// the only PIT mode supported is square wave generator with a configurable
// frequency.
#include <timers/pit.hpp>
#include <util/assert.hpp>

namespace Timer::Pit {

// Command/Mode port of the PIT.
static constexpr Cpu::Port CommandPort = Cpu::Port(0x43);
// Port corresponding to channel 0 of the PIT.
static constexpr Cpu::Port Channel0Port = Cpu::Port(0x40);

// Since the PIT's counter can only hold a 16-bit value, the min frequency
// attainable with the PIT is 1193182/65535 ~= 18 Hz.
// Moreover, using a reload value/divisor of 1 is disallowed in some PIT modes,
// hence set the max frequency to 1193182/2 Hz.
// Creating a Pit::Freq just for this would be confusing because we already have
// a Timer::Freq type. Hence just enforce this with asserts in setFrequency.
static const Freq MinPitFreq = Freq(BaseFrequency / 65535);
static const Freq MaxPitFreq = Freq(BaseFrequency / 2);

// Map the PIT interrupts to a particular vector.
// @param vector: The vector to map PIT IRQs to.
void mapToVector(Interrupts::Vector const vector) {
    Interrupts::mapIrq(Pit::Irq, vector);
}

// Disable the PIT. Note that since there is no way to really disable the PIT
// this function simply configures the I/O APIC to mask the IRQ associated with
// the PIT.
void disable() {
    Interrupts::maskIrq(Pit::Irq);
}

// Configure the PIT to fire an interrupt at the given frequency.
// @param freq: The frequency to configure the PIT to.
void setFrequency(Freq const freq) {
    // The implementation uses the channel 0 of the PIT and configures it to use
    // "Mode 3", e.g. "Square Wave Generator".
    ASSERT(MinPitFreq <= freq && freq <= MaxPitFreq);

    // Configure channel 0 to use mode 3 with access mode lobyte/hibyte.
    u8 const cmdPayload((3 << 4) | (3 << 1));
    Cpu::outb(CommandPort, cmdPayload);

    // Write the reload value.
    u16 const reloadValue(BaseFrequency / freq.raw());
    Cpu::outb(Channel0Port, reloadValue & 0xff);
    Cpu::outb(Channel0Port, reloadValue >> 8);
}
}
