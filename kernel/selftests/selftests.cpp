// Kernel runtime self-tests.
#include <selftests/selftests.hpp>
#include <logging/log.hpp>

namespace SelfTests {

// Create a TestRunner.
TestRunner::TestRunner() : m_numTestsRan(0), m_numTestsPassed(0) {}

// Run a single test with the TestRunner. You typically want to use the RUN_TEST
// macro defined below instead as it automatically figures out the name of the
// test from the func argument.
// @param testName: The name of the test.
// @param testFunc: The test function to run.
void TestRunner::_runTest(char const * const testName,
                          TestFunction const& testFunc) {
    TestResult const result(testFunc());
    m_numTestsRan++;
    if (result == TestResult::Success) {
        m_numTestsPassed++;
        Log::info("  [ OK ] {}", testName);
    } else if (result == TestResult::Failure) {
        Log::crit("  [FAIL] {}", testName);
    } else {
        // result == TestResult::Skip.
        m_numTestsSkipped++;
        Log::warn("  [SKIP] {}", testName);
    }
}

// Print a summary of the passed and failed tests.
void TestRunner::printSummary() const {
    bool const allPassed(m_numTestsRan == m_numTestsPassed);
    if (allPassed) {
        Log::info("All {} tests passed!", m_numTestsRan);
    } else {
        u64 const numFail(m_numTestsRan - m_numTestsPassed - m_numTestsSkipped);
        if (numFail) {
            Log::crit("{} tests passed, {} tests failed, {} tests skipped",
                      m_numTestsPassed, numFail, m_numTestsSkipped);
        } else {
            // Some tests were skipped but all the others passed.
            Log::info("{} tests passed, {} tests skipped", m_numTestsPassed,
                      m_numTestsSkipped);
        }
    }
}
}
