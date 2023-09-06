// Functions to interact with and configure the LAPIC timer.
#include <timers/lapictimer.hpp>
#include <timers/pit.hpp>
#include <interrupts/vectormap.hpp>

namespace Timer::LapicTimer {

// The frequency at which the LAPIC timer decrements the timer count. It is
// assumed that this frequency is the same on all cores hence only computed
// once.
static Freq LapicTimerBaseFreq = Freq(1);

// The frequency at which the LAPIC timer is currently configured.
static Freq LapicTimerCurrFreq = Freq(1);

// Compute the frequency of the LAPIC timer.
// @return: The frequency of the LAPIC timer.
static Freq getTimerFreq() {
    // The LAPIC timer frequency is computed using the PIT as follows:
    //  1. Configure the PIT to a particular frequency.
    //  2. Setup the LAPIC timer so that it does not generate an interrupt and
    //  uses one-shot mode.
    //  3. Set the LAPIC timer's initial count to the max u32.
    //  4. Wait for N PIT ticks.
    //  5. See the current count of the LAPIC timer.
    // Using the LAPIC timer's initial and current count, the number of ticks N
    // and the PIT's frequency we can derive the LAPIC timer's frequency:
    //  LAPIC Hz = deltaCount / waitTime
    //           = (initialCount - currentCount) / (N / PITFreq)
    //           = (initialCount - currentCount) * (PITFreq / N)
    // we can choose any of the above equations depending on wether N > PITFreq
    // or not.
    // However it is better to avoid rounding error and make sure N is divisible
    // by PitFreq or vice-versa.

    Interrupts::Vector const pitVector(Interrupts::VectorMap::PitVector);
    Timer::Freq const pitFreq(1000);
    // FIXME: Need something like std::limits.
    u32 const lapicTimerInitCount(u32(~0ULL));
    // The number of PIT ticks to wait for in order to compute the LAPIC
    // frequency.
    u64 const N(100);

    u64 const waitTime((1000 * N) / pitFreq.raw());
    Log::info("Computing LAPIC timer frequency (wait time = {} ms)", waitTime);

    // Configure the LAPIC timer not to fire an interrupt and be in one-shot
    // mode. One-shot mode allows us to detect if we waited too long (e.g. N is
    // too high) by detecting if the timer's current count is zero after the
    // wait loop.
    Interrupts::Lapic::Lvt const timerLvt({
        .mask = true,
        .timerMode = Interrupts::Lapic::Lvt::TimerMode::OneShot,
    });
    Interrupts::lapic().setTimerLvt(timerLvt);
    // We always use divisor of 1.
    Interrupts::lapic().setTimerDivideConfiguration(
        Interrupts::Lapic::TimerDivideConfiguration::DivideBy1);

    // Disable interrupts while setting up the PIT tick handler so that we do
    // not start counting ticks outside of the wait loop.
    Cpu::disableInterrupts();

    // The number of PIT ticks that occured since the start of the wait.
    static u64 numPitTicks(0);

    // Configure a handler for the PIT ticks, increments numPitTicks at each
    // tick.
    auto const pitHandler([](__attribute__((unused)) Interrupts::Vector const v,
                          __attribute__((unused)) Interrupts::Frame const& f) {
        numPitTicks++;
    });
    Interrupts::registerHandler(pitVector, pitHandler);

    // Configure the PIT. This also starts the PIT but at this point we have
    // interrupts disabled.
    Pit::mapToVector(pitVector);
    Pit::setFrequency(pitFreq);

    // Start the LAPIC timer. After this the timer will decrement its current
    // count.
    Interrupts::lapic().setTimerInitialCount(lapicTimerInitCount);

    // Wait for N PIT ticks.
    Cpu::enableInterrupts();
    Log::debug("Wait for N = {} PIT ticks ({} ms)", N, waitTime);
    while (numPitTicks < N);
    Cpu::disableInterrupts();
    Log::debug("Wait over");

    u32 const lapicTimerCurrCount(Interrupts::lapic().timerCurrentCount());
    if (!lapicTimerCurrCount) {
        // The LAPIC timer counter reached zero during the wait, this means that
        // we waited too long! We cannot compute a sensible value from what we
        // have here.
        Log::crit("LAPIC timer counter expired while waiting for PIT ticks");
        Log::crit("The wait loop is too long for the LAPIC timer freq!");
        PANIC("Cannot reliably compute the LAPIC timer frequency");
    }
    u32 const deltaCount(lapicTimerInitCount - lapicTimerCurrCount);
    // Choose the equation that is best depending on whether or not N < PitFreq.
    Timer::Freq const lapicFreq((N < pitFreq.raw()) ?
        (deltaCount * (pitFreq.raw() / N)) :
        (deltaCount / (N / pitFreq.raw())));
    Log::info("LAPIC timer frequency = {} Hz", lapicFreq.raw());
    return lapicFreq;
}

// Initialize the LAPIC timer. The timer is configured to use the given
// frequency and uses vector Interrupts::VectorMap::LapicTimerVector. The timer
// is _not_ started by calling this function.
// @param freq: The frequency to set the timer to.
void init(Freq const freq) {
    // Stop the timer just in case it was already decrementing.
    stop();

    // Compute the base frequency if needed.
    if (LapicTimerBaseFreq == 1) {
        // The LAPIC timer frequency has not been computed yet, compute it now.
        LapicTimerBaseFreq = getTimerFreq();
    }
    // Check that the requested frequency is supported by the LAPIC timer.
    if (LapicTimerBaseFreq < freq) {
        PANIC("Frequency is too high for the LAPIC timer: {}", freq.raw());
    }
    LapicTimerCurrFreq = freq;
}

// Start the LAPIC timer. Must be called after init().
void start() {
    if (LapicTimerCurrFreq == 1) {
        // init() was not called.
        PANIC("Attempt to call LapicTimer::start() before LapicTimer::init()");
    }

    // Stop the timer just in case it was already decrementing.
    stop();
    
    // Don't use any divisor for the counter, we use the full lapic frequency.
    Interrupts::lapic().setTimerDivideConfiguration(
        Interrupts::Lapic::TimerDivideConfiguration::DivideBy1);

    // Figure out the reload count.
    u32 const reloadCount(LapicTimerBaseFreq.raw() / LapicTimerCurrFreq.raw());
    Interrupts::lapic().setTimerInitialCount(reloadCount);

    // Write the timer LVT.
    Interrupts::Lapic::Lvt const lvt({
        .vector = Interrupts::VectorMap::LapicTimerVector,
        .mask = false,
        .timerMode = Interrupts::Lapic::Lvt::TimerMode::Periodic
    });
    Interrupts::lapic().setTimerLvt(lvt);

    // Start the timer.
    Interrupts::lapic().setTimerInitialCount(reloadCount);
}

// Stop the LAPIC timer.
void stop() {
    Interrupts::lapic().setTimerInitialCount(0);
}
}
