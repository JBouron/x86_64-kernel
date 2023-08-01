// Kernel runtime self-tests.
#pragma once
#include <logging/log.hpp>

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
            Log::crit("    Test assert failed: {}", condStr);   \
            return SelfTests::TestResult::Failure;              \
        }                                                       \
    } while (0)

}
