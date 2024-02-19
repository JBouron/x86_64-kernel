// Locking related types and functions.
#pragma once
#include <concurrency/atomic.hpp>

namespace Concurrency {

// Interface of a lock. All lock implementation derive from this type.
class Lock {
public:
    // Acquire the lock. Only return once the lock has been acquired.
    // @param disableIrq: If true, interrupts are disabled on the cpu until the
    // lock is released. The default is conservative and disables interrupts.
    void lock(bool const disableIrq = true);

    // Check if this lock is currently locked.
    // @return: true if locked, false otherwise.
    virtual bool isLocked() const = 0;

    // Unlock the lock. Must be called by the owner of the lock.
    void unlock();

private:
    // Implementation of the lock and unlock operations. To be implemented by
    // the sub-type.
    virtual void doLock() = 0;
    virtual void doUnlock() = 0;

    // True if the interrupts were disabled while holding the lock.
    bool volatile m_disableIrq;
    // The value of the interrupt flag on the cpu before acquiring the lock.
    bool volatile m_savedIrqFlag;
};

// RAII-style class to automatically lock and unlock a lock within a scope.
class LockGuard {
public:
    // Construct a LockGuard around a lock. This constructor automatically
    // acquires the lock.
    // @param lock: The lock to automatically acquire and release.
    // @param disableIrq: If true, interrupts are disabled on the cpu until the
    // lock is released.
    LockGuard(Lock& lock, bool const disableIrq = true) : m_lock(lock) {
        m_lock.lock(disableIrq);
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
    SpinLock() = default;
    // Copy-constructor. FIXME: This should be removed, there is no reason to
    // ever copy a SpinLock. However, this is currently needed because Vector<T>
    // requires a copy-constructor when growing its array.
    SpinLock(SpinLock const& other);

    virtual bool isLocked() const;
private:
    virtual void doLock();
    virtual void doUnlock();

    Atomic<u8> m_flag;
};

}
