// Kernel runtime self-tests.

#pragma once
#include <logging/log.hpp>

namespace SelfTests {

// Run all runtime kernel self-tests.
void runSelfTests();

// The result of a test run.
using TestResult = bool;
TestResult const TEST_SUCCESS = true;
TestResult const TEST_FAILURE = false;

// A test function is a function taking no arguments and returning a TestResult.
// If the test is passing it should return TEST_SUCCESS, otherwise it should
// return TEST_FAILURE.
using TestFunction = TestResult(*)();

// Run a single test function in a test harness. Output the result of the test.
// This macro is only meant to be used by actual test code.
// @param testFunc: The test function to run.
#define runTest(test)               \
    do {                            \
        extern TestResult test();   \
        _runTest(#test, test);      \
    } while (0)

// Helper function for the runTest macro.
// @param testName: The name of the test.
// @param testFunc: The test function to run.
void _runTest(char const * const testName, TestFunction const& test);

// Some helper macros for tests.

// Assert on a condition. If the condition is false, logs an assert failure and
// return TEST_FAILURE.
// @param cond: The condition to assert on.
#define TEST_ASSERT(cond)                                       \
    do {                                                        \
        if (!(cond)) {                                          \
            char const * const condStr(#cond);                  \
            Log::crit("    Test assert failed: {}", condStr);   \
            return TEST_FAILURE;                                \
        }                                                       \
    } while (0)

}
