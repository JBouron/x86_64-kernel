// Kernel runtime self-tests.
#pragma once
#include <logging/log.hpp>
#include <util/panic.hpp>

namespace SelfTests {

// The result of a test run.
enum class TestResult {
    Success,
    Failure,
};

// Forward decl for TestFunction.
class TestRunner;
// A test function. Runs a test and indicates, through the return value, if the
// test was successful or not.
using TestFunction = TestResult (*)(void);

// Helper class to run tests and gather statistics, e.g. number of tests passed,
// failed, ...
class TestRunner {
public:
    // Create a TestRunner.
    TestRunner();

    // Run a single test with the TestRunner. You typically want to use the
    // RUN_TEST macro defined below instead as it automatically figures out the
    // name of the test from the func argument.
    // @param testName: The name of the test.
    // @param testFunc: The test function to run.
    void _runTest(char const * const testName, TestFunction const& func);

    // Print a summary of the passed and failed tests.
    void printSummary() const;

private:
    // The total number of tests ran so far.
    u64 m_numTestsRan;
    // The total number of tests that passed so far.
    u64 m_numTestsPassed;
};

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
            PANIC("    Test assert failed: {}", condStr);   \
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

}
