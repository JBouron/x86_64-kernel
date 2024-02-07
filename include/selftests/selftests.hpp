// Kernel runtime self-tests.
#pragma once

#include <util/ints.hpp>

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
}
