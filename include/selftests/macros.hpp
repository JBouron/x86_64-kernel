// Useful macros to use within tests.
#pragma once

#include <logging/log.hpp>
#include <util/panic.hpp>
#include <timers/lapictimer.hpp>
#include <interrupts/interrupts.hpp>

// Run a single test function with the TestRunner.
// @param testRunner: Reference to the TestRunner instance under which the test
// should be ran.
// @param testFunc: The test function to run.
#define RUN_TEST(testRunner, testFunc)              \
    do {                                            \
        testRunner._runTest(#testFunc, testFunc);   \
    } while (0)

// Some helper macros for test functions.

// Assert on a condition. If the condition is false, logs an assert failure and
// return TestResult::Failure.
// @param cond: The condition to assert on.
#define TEST_ASSERT(cond)                                       \
    do {                                                        \
        if (!(cond)) {                                          \
            char const * const condStr(#cond);                  \
            PANIC("    Test assert failed: {}", condStr);       \
            return SelfTests::TestResult::Failure;              \
        }                                                       \
    } while (0)

// Wait for a condition to be true. This function uses the LapicTimer to delay
// execution on the current core. If the condition is still not true after the
// timeout logs the failure and return TestResult::Failure.
// @param cond: The condition to wait on.
// @param ms: The max amount of time to wait on the condition in
// milliseconds.
#define TEST_WAIT_FOR(cond, ms)                                               \
    do {                                                                      \
        u64 __remainingIte(Timer::Duration::MilliSecs(ms).microSecs()/10000); \
        while (!(cond) && !!__remainingIte) {                                 \
            Timer::LapicTimer::delay(Timer::Duration::MilliSecs(10));         \
            __remainingIte--;                                                 \
        }                                                                     \
        if (!__remainingIte) {                                                \
            char const * const condStr(#cond);                                \
            Log::crit("    Timeout waiting for: {}", condStr);                \
            return SelfTests::TestResult::Failure;                            \
        }                                                                     \
    } while (0)

// RAII class to temporarily register an interrupt handler for a vector and
// automatically deregister it upon leaving the scope/destruction.
class TemporaryInterruptHandlerGuard {
public:
    // Create a TemporaryInterruptHandlerGuard. Register the handler `handler`
    // for the given vector.
    // @param vector: The vector for which to register a handler.
    // @param handler: The handler to register for `vector`.
    TemporaryInterruptHandlerGuard(Interrupts::Vector const vector,
        Interrupts::InterruptHandler const& handler) : m_vector(vector) {
        Interrupts::registerHandler(m_vector, handler);
    }

    // Deregister the handler upon destruction.
    ~TemporaryInterruptHandlerGuard() {
        Interrupts::deregisterHandler(m_vector);
    }
private:
    Interrupts::Vector const m_vector;
};
