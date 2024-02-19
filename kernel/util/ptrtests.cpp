// Smart pointer declaration.
#include <util/ptr.hpp>
#include <selftests/macros.hpp>
#include <smp/remotecall.hpp>

namespace SmartPtr {

struct Counter {
    u64 numConstruct;
    u64 numDestruct;

    void reset() {
        numConstruct = 0;
        numDestruct = 0;
    }
};

static Counter counter;

class A {
public:
    A(u64 const arg1, u64 const arg2) : arg1(arg1), arg2(arg2) {
        counter.numConstruct++;
    }

    ~A() {
        counter.numDestruct++;
    }

    u64 arg1;
    u64 arg2;
};

// Basic check that Ptr<T> correctly keeps track of the reference count.
SelfTests::TestResult smartPtrTest() {
    // Case #1: Pointer is automatically deleted when going out of scope.
    // Reference count is correct when copying a Ptr<T>.
    counter.reset();
    {
        Ptr<A> const a(Ptr<A>::New(123, 456));
        TEST_ASSERT(counter.numConstruct == 1);
        TEST_ASSERT(counter.numDestruct == 0);
        TEST_ASSERT(a->arg1 == 123);
        TEST_ASSERT(a->arg2 == 456);
        TEST_ASSERT((*a).arg1 == 123);
        TEST_ASSERT((*a).arg2 == 456);
        TEST_ASSERT(a.refCount() == 1);
        TEST_ASSERT(!!a);

        {
            Ptr<A> const a2(a);
            TEST_ASSERT(counter.numConstruct == 1);
            TEST_ASSERT(counter.numDestruct == 0);
            TEST_ASSERT(a2->arg1 == 123);
            TEST_ASSERT(a2->arg2 == 456);
            TEST_ASSERT((*a2).arg1 == 123);
            TEST_ASSERT((*a2).arg2 == 456);
            TEST_ASSERT(a2.refCount() == 2);
            TEST_ASSERT(!!a2);
        }

        TEST_ASSERT(counter.numConstruct == 1);
        TEST_ASSERT(counter.numDestruct == 0);
        TEST_ASSERT(a.refCount() == 1);
    }
    TEST_ASSERT(counter.numConstruct == 1);
    TEST_ASSERT(counter.numDestruct == 1);

    // Case #2: Assignment.
    counter.reset();
    {
        Ptr<A> a;
        {
            Ptr<A> const innerObj(Ptr<A>::New(789, 987));
            TEST_ASSERT(counter.numConstruct == 1);
            TEST_ASSERT(counter.numDestruct == 0);
            a = innerObj;
            TEST_ASSERT(a.refCount() == 2);
            TEST_ASSERT(innerObj.refCount() == 2);
        }
        // Since a reference to the inner object was created, it is not deleted
        // upon leaving the scope.
        TEST_ASSERT(a.refCount() == 1);
        TEST_ASSERT(counter.numConstruct == 1);
        TEST_ASSERT(counter.numDestruct == 0);
        TEST_ASSERT(a->arg1 == 789);
        TEST_ASSERT(a->arg2 == 987);
    }
    TEST_ASSERT(counter.numConstruct == 1);
    TEST_ASSERT(counter.numDestruct == 1);

    return SelfTests::TestResult::Success;
}

// Check that reference counting in Ptr<T> is thread-safe.
SelfTests::TestResult smartPtrConcurrentRefTest() {
    u64 const numRepeat(10);
    u64 const numRefPerCore(1000);
    // Repeat the test case multiple times to catch any eventual intermittent
    // bugs.
    for (u64 rep(0); rep < numRepeat; ++rep) {
        counter.reset();

        // Create Ptr<T>, then make other cpu create `numRefPerCore` references
        // to this same Ptr<T> concurrently. Check that the ref count ends up as
        // expected.
        // After all remote cpu delete their reference, the ref count should go
        // back to 1.
        Ptr<A> const obj(Ptr<A>::New(123, 456));
        TEST_ASSERT(counter.numConstruct == 1);
        TEST_ASSERT(counter.numDestruct == 0);

        Atomic<u64> flag;
        Vector<Smp::RemoteCall::CallResult<void>*> results;

        for (Smp::Id id(0); id < Smp::ncpus(); ++id) {
            if (id == Smp::id()) {
                continue;
            }
            Smp::RemoteCall::CallResult<void>* const call(
                Smp::RemoteCall::invokeOn(id, [&]() {
                Vector<Ptr<A>> refVec;
                for (u64 i(0); i < numRefPerCore; ++i) {
                    refVec.pushBack(Ptr<A>(obj));
                }

                // Wait for the cpu running the test to indicate the end of the
                // test.
                while (!flag.read()) {
                    // FIXME: Add a wait on Atomic<T>.
                    asm("pause");
                }
            }));
            results.pushBack(call);
        }

        u64 const expectedRefCount((Smp::ncpus() - 1) * numRefPerCore + 1);
        TEST_WAIT_FOR(obj.refCount() == expectedRefCount, 1000);
        TEST_ASSERT(counter.numConstruct == 1);
        TEST_ASSERT(counter.numDestruct == 0);

        // Let the remote cpu finish their remote invocation.
        flag++;

        for (u64 i(0); i < results.size(); ++i) {
            results[i]->wait();
            delete results[i];
        }

        TEST_ASSERT(obj.refCount() == 1);
    }
    return SelfTests::TestResult::Success;
}

// Run the tests for the Ptr<T> type.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, smartPtrTest);
    RUN_TEST(runner, smartPtrConcurrentRefTest);
}

}
