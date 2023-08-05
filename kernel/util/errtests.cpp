// Tests for the Err type.
#include <util/err.hpp>

namespace ErrType {

SelfTests::TestResult errBasicTest() {
    Err const noError(Ok);
    TEST_ASSERT(!noError);
    Err const withError(Error::Test);
    TEST_ASSERT(!!withError);
    TEST_ASSERT(withError.error() == Error::Test);
    Err const defaultErr;
    TEST_ASSERT(!defaultErr);
    return SelfTests::TestResult::Success;
}

void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, errBasicTest);
}
}
