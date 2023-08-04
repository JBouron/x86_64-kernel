// Tests for the Res<T> type.
#include <util/result.hpp>

namespace Result {

// Basic test creating a Res<int> with or without an error.
SelfTests::TestResult resultBasicTest() {
    Res<int> const withValue(123);
    TEST_ASSERT(withValue.ok());
    TEST_ASSERT(!!withValue);
    TEST_ASSERT(withValue.value() == 123);

    Res<int> const withError(Error::Test);
    TEST_ASSERT(!withError.ok());
    TEST_ASSERT(!withError);
    TEST_ASSERT(withError.error() == Error::Test);

    return SelfTests::TestResult::Success;
}

// Enumeration indicating which constructor was called when constructing the
// a TestClass object.
enum class CalledConstructor {
    // No constructor called.
    None,
    // The default constructor was called.
    Default,
    // The TestClass(int const) was called.
    Direct,
    // The copy constructor was called.
    Copy,
};

// Constructor called when constructing the last TestClass object.
static CalledConstructor lastCalledConstructor = CalledConstructor::None;

// Indicate if the ~TestClass destructor has been called since the
// reset of this value. Set in ~TestClass().
static bool destructorCalled = false;

// Class for the optConstructorTest tests. Saves which constructor was called in
// a member.
class TestClass {
public:
    TestClass() : m_value(0) {
        lastCalledConstructor = CalledConstructor::Default;
    }

    TestClass(int const value) : m_value(value) {
        lastCalledConstructor = CalledConstructor::Direct;
    }

    TestClass(TestClass const& other) : m_value(other.m_value) {
        lastCalledConstructor = CalledConstructor::Copy;
    }

    ~TestClass() {
        destructorCalled = true;
    }

    int getValue() const {
        return m_value;
    }

private:
    int const m_value;
};

// Check that the Res<T> calls the correct constructor when constructing the
// embedded value.
SelfTests::TestResult resultConstructorTest() {
    // Test case 1: Creating a Res with an error does not call any
    // constructor on the value type.
    lastCalledConstructor = CalledConstructor::None;
    Res<TestClass> const res0(Error::Test);
    TEST_ASSERT(lastCalledConstructor == CalledConstructor::None);
    TEST_ASSERT(!res0.ok());

    // Test case 2: Default constructor of Res<T> creates a default value
    // through its default constructor.
    lastCalledConstructor = CalledConstructor::None;
    Res<TestClass> const res1;
    TEST_ASSERT(lastCalledConstructor == CalledConstructor::Default);
    TEST_ASSERT(res1.ok());
    TEST_ASSERT(res1.value().getValue() == 0x0);
    
    // Test case 3: In-place construction of value.
    lastCalledConstructor = CalledConstructor::None;
    Res<TestClass> res2(1234);
    TEST_ASSERT(lastCalledConstructor == CalledConstructor::Direct);
    TEST_ASSERT(res2.ok());
    TEST_ASSERT(res2.value().getValue() == 1234);

    // Test case 4: Copy construction of value.
    TestClass const value(123);
    lastCalledConstructor = CalledConstructor::None;
    Res<TestClass> const res3(value);
    TEST_ASSERT(lastCalledConstructor == CalledConstructor::Copy);
    TEST_ASSERT(res3.ok());
    TEST_ASSERT(res3.value().getValue() == 123);

    return SelfTests::TestResult::Success;
}

// Check that the Res<T> correctly calls the destructor of the contained
// value if it contains one.
SelfTests::TestResult resultDestructorTest() {
    // Test-case #1: The Res<T> contains an error.
    destructorCalled = false;
    {
        Res<TestClass> res(Error::Test);
    }
    TEST_ASSERT(!destructorCalled);

    // Test-case #2: The Res<T> contains a value.
    destructorCalled = false;
    {
        Res<TestClass> res(1234);
    }
    TEST_ASSERT(destructorCalled);

    return SelfTests::TestResult::Success;
}

// Check using the * and -> operators on a Res<T>.
SelfTests::TestResult resultMemberFunctionTest() {
    Res<TestClass> res(0xdead);
    Res<TestClass> const resConst(0xbeef);
    TEST_ASSERT((*res).getValue() == 0xdead);
    TEST_ASSERT((*resConst).getValue() == 0xbeef);
    TEST_ASSERT(res->getValue() == 0xdead);
    TEST_ASSERT(resConst->getValue() == 0xbeef);
    return SelfTests::TestResult::Success;
}

// Run the tests for the Res<T> type.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, resultBasicTest);
    RUN_TEST(runner, resultConstructorTest);
    RUN_TEST(runner, resultDestructorTest);
    RUN_TEST(runner, resultMemberFunctionTest);
}
}
