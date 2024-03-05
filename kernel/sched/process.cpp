// Process representation and manipulation.
#include <sched/process.hpp>
#include <memory/stack.hpp>
#include <smp/smp.hpp>
#include <util/panic.hpp>
#include <cpu/cpu.hpp>
#include <concurrency/atomic.hpp>
#include "./schedimpl.hpp"

namespace Sched {

// Catch a process running a function that returned from that function. This
// raises a PANIC.
static void handleRetFromProc() {
    // FIXME: For now, we don't have a pointer on Proc object associated with
    // the current process, hence cannot print its ID.
    PANIC("Process returned from its function call");
}

// Create a process that executes a function or lambda.
// @param func: The function to be executed. This must be a function taking no
// argument. If this function ever returns, a PANIC is raised.
// @return: A pointer to the Proc instance or an error, if any.
Res<Ptr<Proc>> Proc::New(void (*func)(void)) {
    Res<Ptr<Proc>> const procAlloc(Proc::New());
    if (!procAlloc) {
        return procAlloc.error();
    }
    Ptr<Proc> const proc(procAlloc.value());

    // Setup the stack with three stack frames:
    //  1. A stack frame for handleRetFromProc.
    //  2. A stack frame for func.
    //  3. A stack frame for contextSwitch.
    // The goal here is to make it seem that the process called
    // contextSwitch from func which itself was called from handleRetFromProc.
    u64* stackPtr(proc->m_kernelStack->highAddress().ptr<u64>());
    // Frame 1: Only need to push the return address to handleRetFromProc.
    *(--stackPtr) = reinterpret_cast<u64>(handleRetFromProc);
    // Frame 2: Only need to push the return address to func.
    *(--stackPtr) = reinterpret_cast<u64>(func);
    // Frame 3: Push the saved callee-saved registers since those are popped by
    // contextSwitch, ie. we are pretending that this stack has ran
    // contextSwitch before. For now the value of the register are un-important,
    // leave them 0x0.
    for (u64 i(0); i < 6; ++i) {
        *(--stackPtr) = 0;
    }

    proc->m_savedKernelStackPointer = reinterpret_cast<u64>(stackPtr);
    // Processes executing a function are always ready for execution.
    proc->m_state = State::Ready;
    return proc;
}

// Get the unique identifier associated with this process.
Proc::Id Proc::id() const {
    return m_id;
}

// Get the current state of the process.
Proc::State Proc::state() const {
    return m_state;
}

// Set the current state of the process. Only the following transitions are
// allowed:
//   - Blocked -> Ready: The operation that was blocking this process has
//   completed. The process is now ready to be scheduled.
//   - Ready -> Running: The process has been scheduled on a cpu. It is now
//   executing instructions.
//   - Running -> Ready: The process has been de-scheduled from the cpu it was
//   running on, ie. it has been preempted. The process is still ready to run,
//   ie not blocked.
//   - Running -> Blocked: The process was running but is now waiting on some
//   operation to complete.
// Any other transition is disallowed and raises a PANIC as it is most likely
// due to a bug.
// @param newState: The state to put the process in.
void Proc::setState(Proc::State const& newState) {
    bool const isValidTransition(
        (m_state == State::Blocked && newState == State::Ready)
        || (m_state == State::Ready && newState == State::Running)
        || (m_state == State::Running && newState == State::Ready)
        || (m_state == State::Running && newState == State::Blocked));
    if (!isValidTransition) {
        PANIC("Invalid state transition for proc {}", m_id);
    }
    m_state = newState;
}

// Create a process.
// @return: A pointer to the Proc instance or an error, if any.
Res<Ptr<Proc>> Proc::New() {
    Res<Ptr<Memory::Stack>> const stackAllocRes(Memory::Stack::New());
    if (!stackAllocRes) {
        return stackAllocRes.error();
    }
    Ptr<Memory::Stack> const kernelStack(stackAllocRes.value());

    Res<Ptr<Paging::AddrSpace>> const addrSpaceAlloc(Paging::AddrSpace::New());
    if (!addrSpaceAlloc) {
        return addrSpaceAlloc.error();
    }
    Ptr<Paging::AddrSpace> const addrSpace(addrSpaceAlloc.value());

    Ptr<Proc> const proc(Ptr<Proc>::New(addrSpace, kernelStack));
    if (!proc) {
        return Error::MaxHeapSizeReached;
    } else {
        return proc;
    }
}

// Process ID allocation:
// Proc::Id are always monitically increasing and generated during within the
// Proc() constructor.

// The smallest Proc::Id possible.
static Proc::Id processIdLow(0);
// The next available Proc:Id.
static Atomic<Proc::Id> nextProcessId(processIdLow);
// Create a new Proc::Id in a thread-safe manner. Process IDs are monitically
// increasing.
static Proc::Id allocateProcessId() {
    return Proc::Id(nextProcessId++);
}

// Create a process. The process starts in the blocked state.
// @param addrSpace: The address space of the process.
// @param kernelStack: The kernel stack to be used by this process.
Proc::Proc(Ptr<Paging::AddrSpace> const& addrSpace,
           Ptr<Memory::Stack> const& kernelStack) :
    m_id(allocateProcessId()),
    m_addrSpace(addrSpace),
    m_kernelStack(kernelStack),
    m_savedKernelStackPointer(kernelStack->highAddress()),
    m_state(State::Blocked) {}

// Jump to the context of the given process. This function does not save the
// current context and does not return! This is only meant to be used when
// switching from the boot satck/context to the very first process running on a
// cpu since at that point there is no Proc associated with the current context.
// @param to: The process to switch to. This function asserts that this process
// is in the Ready state and updates its state to Running.
void Proc::jumpToContext(Ptr<Proc> const& to) {
    // Save the current stack pointer on a dummy variable, we will _NOT_ return
    // to this stack ever.
    ASSERT(to->state() == State::Ready);
    u64 dummy;
    to->setState(State::Running);
    Paging::AddrSpace::switchAddrSpace(to->m_addrSpace);
    Sched::contextSwitch(to->m_savedKernelStackPointer, &dummy);
}

// Switch from the current context to another process' context and address
// space.
// @param curr: The Proc associated with the current context. This function
// saves the current context in this Proc. This function asserts that this
// process is in either the Running or Blocked state. If it is in the Running
// state, the state is updated to Ready, otherwise the state is untouched and
// remains in Blocked.
// @param to: The process to switch execution to. This function asserts that
// this process is in the Ready state and updates its state to Running.
void Proc::contextSwitch(Ptr<Proc> const& curr, Ptr<Proc> const& to) {
    State const currState(curr->state());
    ASSERT(currState == State::Running || currState == State::Blocked);
    ASSERT(to->state() == State::Ready);
    curr->setState(currState == State::Running ? State::Ready : State::Blocked);
    to->setState(State::Running);
    Paging::AddrSpace::switchAddrSpace(to->m_addrSpace);
    Sched::contextSwitch(to->m_savedKernelStackPointer,
                         &curr->m_savedKernelStackPointer);
}
}
