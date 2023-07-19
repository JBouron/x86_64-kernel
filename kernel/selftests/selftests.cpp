// Kernel runtime self-tests.

#include <selftests/selftests.hpp>

namespace SelfTests {

// Self tests statistics.
struct TestStats {
    // Number of tests ran so far.
    u64 numTests;
    // Number of tests that successfully passed so far.
    u64 numPassed;
};
static TestStats TEST_STATS;

// Run all runtime kernel self-tests.
void runSelfTests() {
    Log::info("Running self-tests:");

    // Add calls to specific tests or other functions calling tests below. The
    // only requirement is that individual tests are ran using the runTest
    // macro.

    runTest(lgdtSgdtTest);


    bool const allPassed(TEST_STATS.numTests == TEST_STATS.numPassed);
    if (allPassed) {
        Log::info("All {} tests passed!", TEST_STATS.numTests);
    } else {
        u64 const numFailed(TEST_STATS.numTests - TEST_STATS.numPassed);
        Log::crit("{} / {} tests failed!", numFailed, TEST_STATS.numTests);
    }
}

// Run a single test function in a test harness. Output the result of the test.
// This function is only meant to be used by actual test code.
// @param test: The test function to run.
void _runTest(char const * const testName, TestFunction const& testFunc) {
    bool const isPassing(testFunc() == TEST_SUCCESS);
    TEST_STATS.numTests++;
    if (isPassing) {
        TEST_STATS.numPassed++;
        Log::info("  [ OK ] {}", testName);
    } else {
        Log::crit("  [FAIL] {}", testName);
    }
}

}
