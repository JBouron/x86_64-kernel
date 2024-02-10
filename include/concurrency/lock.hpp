// Locking related types and functions.
#pragma once
#include <concurrency/atomic.hpp>

namespace Concurrency {

// Interface of a lock. All lock implementation derive from this type.
class Lock {
public:
    // Acquire the lock. Only return once the lock has been acquired.
    virtual void lock() = 0;

    // Check if this lock is currently locked.
    // @return: true if locked, false otherwise.
    virtual bool locked() const = 0;

    // Unlock the lock. Must be called by the owner of the lock.
    virtual void unlock() = 0;
};

// RAII-style class to automatically lock and unlock a lock within a scope.
class LockGuard {
public:
    // Construct a LockGuard around a lock. This constructor automatically
    // acquires the lock.
    // @param lock: The lock to automatically acquire and release.
    LockGuard(Lock& lock) : m_lock(lock) {
        m_lock.lock();
    }

    // Upon destruction, automatically unlock the lock that was acquired during
    // construction.
    ~LockGuard() {
        m_lock.unlock();
    }

private:
    Lock& m_lock;
};


// Spinlock implementation.
class SpinLock : public Lock {
public:
    void lock() {
        while (!m_flag.compareAndExchange(0, 1)) {
            asm("pause");
        }
    }

    bool locked() const {
        return m_flag == 1;
    }

    void unlock() {
        // FIXME: Add better check that it is the owner that unlocks the lock.
        ASSERT(locked());
        // Using cmpxchg instead of a simple write makes sure that we are
        // serializing the instruction stream.
        m_flag.compareAndExchange(1, 0);
    }

private:
    Atomic<u8> m_flag;
};

}
