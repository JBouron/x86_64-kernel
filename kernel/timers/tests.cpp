// Timer tests.
#include <timers/pit.hpp>

namespace Timer {

// Test basic functionality of the PIT timer configuration. This function simply
// sets the timer to fire an interrupts with a particular vector and checks that
// the handler for this vector is called. It then disable the timer and asserts
// (with somewhat certainty) that the PIT is disabled.
// Unfortunately there is not really a way to properly test the frequency
// configuration, this will have to be done manually.
SelfTests::TestResult pitBasicTest() {
    // The test vector to be used, this vector should be free.
    Interrupts::Vector const vector(32);
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
    Interrupts::registerHandler(vector, handler);

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

    Interrupts::deregisterHandler(vector);
    return SelfTests::TestResult::Success;
}

// Run Timer tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, pitBasicTest);
}
}
