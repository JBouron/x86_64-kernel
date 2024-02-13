// Timer tests.
#include <timers/pit.hpp>
#include <timers/lapictimer.hpp>
#include <interrupts/vectormap.hpp>
#include <selftests/macros.hpp>

namespace Timer {

// Test basic functionality of the PIT timer configuration. This function simply
// sets the timer to fire an interrupts with a particular vector and checks that
// the handler for this vector is called. It then disable the timer and asserts
// (with somewhat certainty) that the PIT is disabled.
// Unfortunately there is not really a way to properly test the frequency
// configuration, this will have to be done manually.
SelfTests::TestResult pitBasicTest() {
    // The test vector to be used, this vector should be free.
    Interrupts::Vector const vector(Interrupts::VectorMap::TestVector);
    // The higher the frequency the more certain the test on the Pit::disable.
    Timer::Freq const freq(100);
    Pit::setFrequency(freq);

    // The number of ticks, e.g. interrupts, received from the PIT during this
    // tests.
    static u64 numTicks(0);
    numTicks = 0;

    // Handler for the PIT interrupts. This merely increments the numTicks
    // variable.
    auto const handler([](__attribute__((unused)) Interrupts::Vector const v,
                          __attribute__((unused)) Interrupts::Frame const& f) {
        numTicks += 1;
    });
    // FIXME: Need a way to register a handler that does not care about the
    // vector and / or the frame.
    TemporaryInterruptHandlerGuard guard(vector, handler);

    // Map the PIT IRQ to the test vector. After this function the handler will
    // be invoked.
    Pit::mapToVector(vector);
    Log::debug("PIT IRQ is mapped, if the kernel hangs here then there is a bug"
               " with the IRQ mapping for the PIT");
    u64 const startEnabled(Cpu::rdtsc());
    while(numTicks < 10) {
        asm("sti");
        asm("hlt");
    }
    u64 const endEnabled(Cpu::rdtsc());

    // The number of cycles to wait for the Pit::disable() test.
    u64 const waitForDisabled(endEnabled - startEnabled);

    // Now test that the PIT can be "disabled". To do so we disable the PIT then
    // we wait for roughly the same number of cycles it took to receive 10
    // ticks. We then check that during the wait not a single tick has been
    // received.
    Pit::disable();
    // Re-read the numTicks because there might have been more PIT interrupts
    // between the last loop and the ::disable().
    u64 const numTicksBefore(numTicks);
    u64 const startDisabled(Cpu::rdtsc());
    // Wait for at least waitForDisabled cycles.
    Log::debug("Wait for {} cycles to detect PIT IRQ", waitForDisabled);
    while(Cpu::rdtsc() - startDisabled < waitForDisabled) {
        asm("pause");
    }
    TEST_ASSERT(numTicks == numTicksBefore);
    return SelfTests::TestResult::Success;
}

// Test basic functionality of the LAPIC timer timer configuration. This
// function simply sets the timer to fire an interrupts with a particular vector
// and checks that the handler for this vector is called. It then stops the
// timer and asserts (with somewhat certainty) that the LAPIC timer is stopped.
// Unfortunately there is not really a way to properly test the frequency
// configuration, this will have to be done manually.
SelfTests::TestResult lapicBasicTest() {
    // The test vector to be used. The vector for the LAPIC timer is not
    // configurable so use the default one.
    Interrupts::Vector const vector(Interrupts::VectorMap::LapicTimerVector);
    // The higher the frequency the more certain the test on the
    // LapicTimer::stop.
    Timer::Freq const freq(100);
    LapicTimer::init(freq);

    // The number of ticks, e.g. interrupts, received from the LAPIC timer
    // during this tests.
    static u64 numTicks(0);
    numTicks = 0;

    // Handler for the PIT interrupts. This merely increments the numTicks
    // variable.
    auto const handler([](__attribute__((unused)) Interrupts::Vector const v,
                          __attribute__((unused)) Interrupts::Frame const& f) {
        numTicks += 1;
    });
    // FIXME: Need a way to register a handler that does not care about the
    // vector and / or the frame.
    TemporaryInterruptHandlerGuard guard(vector, handler);

    // Start the LAPIC timer.
    LapicTimer::start();
    Log::debug("Waiting for LAPIC timer ticks, if the kernel hangs here then "
               "there is a bug with the LAPIC timer.");
    u64 const startEnabled(Cpu::rdtsc());
    while(numTicks < 10) {
        asm("sti");
        asm("hlt");
    }
    u64 const endEnabled(Cpu::rdtsc());

    // The number of cycles to wait for the LapicTimer::stop() test.
    u64 const waitForDisabled(endEnabled - startEnabled);

    // Now test that the LAPIC timer can be stopped. To do so we disable the PIT
    // then we wait for roughly the same number of cycles it took to receive 10
    // ticks. We then check that during the wait not a single tick has been
    // received.
    LapicTimer::stop();
    // Re-read the numTicks because there might have been more ticks between the
    // last loop and the ::stop().
    u64 const numTicksBefore(numTicks);
    u64 const startDisabled(Cpu::rdtsc());
    // Wait for at least waitForDisabled cycles.
    Log::debug("Wait for {} cycles to detect LAPIC timer INT", waitForDisabled);
    while(Cpu::rdtsc() - startDisabled < waitForDisabled) {
        asm("pause");
    }
    TEST_ASSERT(numTicks == numTicksBefore);
    return SelfTests::TestResult::Success;
}

// Run Timer tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, pitBasicTest);
    RUN_TEST(runner, lapicBasicTest);
}
}
