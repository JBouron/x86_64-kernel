// Locking related types and functions.
#include <concurrency/lock.hpp>
#include <cpu/cpu.hpp>
#include <util/assert.hpp>

namespace Concurrency {
// Acquire the lock. Only return once the lock has been acquired.
// @param disableIrq: If true, interrupts are disabled on the cpu until the lock
// is released. The default is conservative and disables interrupts.
void Lock::lock(bool const disableIrq) {
    bool const irqFlag(Cpu::interruptsEnabled());
    if (disableIrq) {
        Cpu::disableInterrupts();
    }

    doLock();

    m_savedIrqFlag = irqFlag;
    m_disableIrq = disableIrq;
}

// Unlock the lock. Must be called by the owner of the lock.
void Lock::unlock() {
    bool const irqDisabled(m_disableIrq);
    bool const savedIrq(m_savedIrqFlag);

    doUnlock();

    if (irqDisabled) {
        Cpu::setInterruptFlag(savedIrq);
    }
}

bool SpinLock::isLocked() const {
    return m_flag == 1;
}

void SpinLock::doLock() {
    while (!m_flag.compareAndExchange(0, 1)) {
        asm("pause");
    }
}

void SpinLock::doUnlock() {
    // FIXME: Add better check that it is the owner that unlocks the lock.
    ASSERT(isLocked());
    // Using cmpxchg instead of a simple write makes sure that we are
    // serializing the instruction stream.
    m_flag.compareAndExchange(1, 0);
}
}
