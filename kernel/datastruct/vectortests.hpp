#pragma once
#include <selftests/selftests.hpp>

namespace DataStruct {
SelfTests::TestResult vectorDefaultConstructionTest();
SelfTests::TestResult vectorConstructorSizeDefaultValueTest();
SelfTests::TestResult vectorConstructorSizeWithValueTest();
SelfTests::TestResult vectorDestructorTest();
SelfTests::TestResult vectorAccessorTest();
SelfTests::TestResult vectorClearTest();
SelfTests::TestResult vectorPushBackTest();
SelfTests::TestResult vectorPopBackTest();
SelfTests::TestResult vectorInsertFrontTest();
SelfTests::TestResult vectorInsertMiddleTest();
SelfTests::TestResult vectorEraseTest();
SelfTests::TestResult vectorIteratorTest();
SelfTests::TestResult vectorCopyTest();
SelfTests::TestResult vectorAssignTest();
SelfTests::TestResult vectorComparisonTest();
}
