// Tests for the RemoteCall::invokeOn() function.
#include <smp/remotecall.hpp>
#include <selftests/macros.hpp>

namespace Smp::RemoteCall {

// Check that a remote call executes on the remote cpu.
SelfTests::TestResult remoteCallBasicTest() {
    for (Smp::Id id(0); id < Smp::ncpus(); ++id) {
        if (id == Smp::id()) {
            continue;
        }
        Ptr<CallResult<Smp::Id>> call(invokeOn(id, []() { return Smp::id(); }));
        TEST_ASSERT(call->returnValue() == id);
    }
    return SelfTests::TestResult::Success;
}

// Check that specifying a capture list on the lambda passed to invokeOn behaves
// as expected.
SelfTests::TestResult remoteCallCaptureListTest() {
    for (Smp::Id id(0); id < Smp::ncpus(); ++id) {
        if (id == Smp::id()) {
            continue;
        }
        u64 var1(0);
        u64 var2(0);

        Ptr<CallResult<void>> call(invokeOn(id, [&]() {
            var1 = 0xdeadbeefcafebabe;
            u64 * const ptr(&var2);
            *ptr = 0xbeefbabedeadcafe;
        }));
        call->wait();
        TEST_ASSERT(var1 == 0xdeadbeefcafebabe);
        TEST_ASSERT(var2 == 0xbeefbabedeadcafe);
    }
    return SelfTests::TestResult::Success;
}

// Check passing arguments to a remote call.
SelfTests::TestResult remoteCallArgsTest() {
    for (Smp::Id id(0); id < Smp::ncpus(); ++id) {
        if (id == Smp::id()) {
            continue;
        }
        u64 const val1(0xbeefbabedeadcafe);
        u64 const val2(0xabbaabbaabbaabba);
        Ptr<CallResult<u64>> call(invokeOn(id,[](u64 const a,u64 const b) {
            return Smp::id().raw() * a + b;
        }, val1, val2));
        call->wait();
        TEST_ASSERT(call->returnValue() == val1 * id.raw() + val2);
    }
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult remoteCallWaitTest() {
    Smp::Id const destCpu((Smp::id().raw() + 1) % Smp::ncpus());
    Atomic<u8> flag;
    Ptr<CallResult<void>> const call(invokeOn(destCpu, [&]() {
        while (!flag.read()) {
            // FIXME: Add a wait on Atomic<T>.
            asm("pause");
        }
    }));
    TEST_ASSERT(!call->isDone());
    Timer::LapicTimer::delay(Timer::Duration::MilliSecs(500));
    TEST_ASSERT(!call->isDone());
    flag++;
    call->wait();
    TEST_ASSERT(call->isDone());
    return SelfTests::TestResult::Success;
}

// Check that remote calls are enqueued at the destination and called in the
// order they were enqueued.
SelfTests::TestResult remoteCallQueueTest() {
    // Repeat the test case multiple times to catch any eventual intermittent
    // bugs.
    u64 const numRepeat(10);
    for (u64 rep(0); rep < numRepeat; ++rep) {
        u64 const numCalls(100);
        Smp::Id const destCpu((Smp::id().raw() + 1) % Smp::ncpus());

        Atomic<u8> startFlag;
        Atomic<u64> counter;
        Vector<Ptr<CallResult<u64>>> results;

        for (u64 i(0); i < numCalls; ++i) {
            Ptr<CallResult<u64>> const call(invokeOn(destCpu, [&]() {
                while (!startFlag.read()) {
                    // FIXME: Add a wait on Atomic<T>.
                    asm("pause");
                }
                return counter++;
            }));
            results.pushBack(call);
        }

        // Let the remote cpu start the invocations.
        startFlag++;

        for (u64 i(0); i < numCalls; ++i) {
            TEST_ASSERT(results[i]->returnValue() == i);
        }
    }
    return SelfTests::TestResult::Success;
}

void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, remoteCallBasicTest);
    RUN_TEST(runner, remoteCallCaptureListTest);
    RUN_TEST(runner, remoteCallArgsTest);
    RUN_TEST(runner, remoteCallWaitTest);
    RUN_TEST(runner, remoteCallQueueTest);
}
}
