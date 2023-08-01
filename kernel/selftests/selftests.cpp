// Kernel runtime self-tests.
#include <selftests/selftests.hpp>

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
    bool const isPassing(testFunc() == TestResult::Success);
    m_numTestsRan++;
    if (isPassing) {
        m_numTestsPassed++;
        Log::info("  [ OK ] {}", testName);
    } else {
        Log::crit("  [FAIL] {}", testName);
    }
}

// Print a summary of the passed and failed tests.
void TestRunner::printSummary() const {
    bool const allPassed(m_numTestsRan == m_numTestsPassed);
    if (allPassed) {
        Log::info("All {} tests passed!", m_numTestsRan);
    } else {
        u64 const numFailed(m_numTestsRan - m_numTestsPassed);
        Log::crit("{} / {} tests failed!", numFailed, m_numTestsRan);
    }
}
}
