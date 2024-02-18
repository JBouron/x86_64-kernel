// Some tests related to concurrency.
#include <selftests/macros.hpp>
#include <concurrency/tests.hpp>
#include <concurrency/atomic.hpp>
#include <concurrency/lock.hpp>

namespace Concurrency {

// Execute very _basic_ tests on Atomic<T>. Note that this does NOT test whether
// or not the type is really atomic and thread-safe. This test merely checks
// that the operators are correctly implemented.
SelfTests::TestResult atomicBasicOperatorsTest() {
    // Default initialization.
    TEST_ASSERT(!Atomic<u16>().read());

    // Non-default initialization.
    Atomic<u32> a(123);
    TEST_ASSERT(a.read() == 123);

    // Assignment.
    a.write(321);
    TEST_ASSERT(a.read() == 321);
    a = 456;
    TEST_ASSERT(a.read() == 456);

    // Cmpxchg.
    // If the value is not the expected value, returns false and no exchange
    // takes place.
    TEST_ASSERT(!a.compareAndExchange(123, 888));
    TEST_ASSERT(a.read() == 456);
    // Successful cmpxchg returns true and writes the new value.
    TEST_ASSERT(a.compareAndExchange(456, 999));
    TEST_ASSERT(a.read() == 999);

    a = 1;
    TEST_ASSERT((++a) == 2);
    TEST_ASSERT(a.read() == 2);
    TEST_ASSERT((--a) == 1);
    TEST_ASSERT(a.read() == 1);
    TEST_ASSERT((a++) == 1);
    TEST_ASSERT(a.read() == 2);
    TEST_ASSERT((a--) == 2);
    TEST_ASSERT(a.read() == 1);

    a = 10;
    a += 5;
    TEST_ASSERT(a == 15);
    a -= 7;
    TEST_ASSERT(a == 8);

    return SelfTests::TestResult::Success;
}

// Very basic test for locks.
SelfTests::TestResult spinLockBasicTest() {
    bool const irqFlagSaved(Cpu::interruptsEnabled());
    Cpu::enableInterrupts();

    SpinLock lock;

    // Case #1: Manually calling lock/unlock with disabledIrq = false.
    lock.lock(false);
    TEST_ASSERT(lock.isLocked());
    TEST_ASSERT(Cpu::interruptsEnabled());
    lock.unlock();
    TEST_ASSERT(!lock.isLocked());
    TEST_ASSERT(Cpu::interruptsEnabled());

    // Case #2: Manually calling lock/unlock with disabledIrq = false.
    lock.lock();
    TEST_ASSERT(lock.isLocked());
    TEST_ASSERT(!Cpu::interruptsEnabled());
    lock.unlock();
    TEST_ASSERT(!lock.isLocked());
    TEST_ASSERT(Cpu::interruptsEnabled());

    Cpu::setInterruptFlag(irqFlagSaved);
    return SelfTests::TestResult::Success;
}

// Check that LockGuard acquires and releases the lock as intended.
SelfTests::TestResult lockGuardTest() {
    bool const irqFlagSaved(Cpu::interruptsEnabled());
    Cpu::enableInterrupts();

    SpinLock lock;

    // Case #1: Using a lock guard with disableIrq = false.
    {
        LockGuard guard(lock, false);
        TEST_ASSERT(lock.isLocked());
        TEST_ASSERT(Cpu::interruptsEnabled());
    }
    TEST_ASSERT(!lock.isLocked());
    TEST_ASSERT(Cpu::interruptsEnabled());

    // Case #2: Using a lock guard with disableIrq = true.
    {
        LockGuard guard(lock);
        TEST_ASSERT(lock.isLocked());
        TEST_ASSERT(!Cpu::interruptsEnabled());
    }
    TEST_ASSERT(!lock.isLocked());
    TEST_ASSERT(Cpu::interruptsEnabled());

    Cpu::setInterruptFlag(irqFlagSaved);
    return SelfTests::TestResult::Success;
}

// Run concurrency related tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, atomicBasicOperatorsTest);
    RUN_TEST(runner, spinLockBasicTest);
    RUN_TEST(runner, lockGuardTest);
}

}
