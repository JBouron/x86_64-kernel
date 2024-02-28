// Process representation and manipulation.
#pragma once
#include <util/ints.hpp>
#include <util/addr.hpp>
#include <memory/stack.hpp>
#include <selftests/selftests.hpp>
#include <paging/addrspace.hpp>

namespace Sched {

// A process in the kernel. This process runs in ring 0 and in the same address
// space used to boot the kernel.
class Proc {
public:
    // Type for processes' unique identifiers.
    using Id = u64;

    // Create a process.
    // @param id: The unique identifier of the process.
    // @return: A pointer to the Proc instance or an error, if any.
    static Res<Ptr<Proc>> New(Id const id);

    // Create a process that executes a function or lambda.
    // @param id: The unique identifier of the process.
    // @param func: The function to be executed. This must be a function taking
    // no argument. If this function ever returns, a PANIC is raised.
    // @return: A pointer to the Proc instance or an error, if any.
    static Res<Ptr<Proc>> New(Id const id, void (*func)(void));

    // Get the unique identifier associated with this process.
    Id id() const;

    // The state of a process.
    enum class State {
        // The process is currently running on a cpu.
        Running,
        // The process is ready to be run and waiting to be scheduled on a cpu.
        Ready,
        // The process is not runnable, blocked by an operation.
        Blocked,
    };

    // Get the current state of the process.
    State state() const;

    // Set the current state of the process. Only the following transitions are
    // allowed:
    //   - Blocked -> Ready: The operation that was blocking this process has
    //   completed. The process is now ready to be scheduled.
    //   - Ready -> Running: The process has been scheduled on a cpu. It is now
    //   executing instructions.
    //   - Running -> Ready: The process has been de-scheduled from the cpu it
    //   was running on, ie. it has been preempted. The process is still ready
    //   to run, ie not blocked.
    //   - Running -> Blocked: The process was running but is now waiting on
    //   some operation to complete.
    // Any other transition is disallowed and raises a PANIC as it is most
    // likely due to a bug.
    // @param newState: The state to put the process in.
    void setState(State const& newState);

    // Jump to the context of the given process. This function does not save the
    // current context and does not return! This is only meant to be used when
    // switching from the boot satck/context to the very first process running
    // on a cpu since at that point there is no Proc associated with the current
    // context.
    // @param to: The process to switch to. This function asserts that this
    // process is in the Ready state and updates its state to Running.
    static void jumpToContext(Ptr<Proc> const& to);

    // Switch from the current context to another process' context and address
    // space.
    // @param curr: The Proc associated with the current context. This function
    // saves the current context in this Proc. This function asserts that this
    // process is in either the Running or Blocked state. If it is in the
    // Running state, the state is updated to Ready, otherwise the state is
    // untouched and remains in Blocked.
    // @param to: The process to switch execution to. This function asserts that
    // this process is in the Ready state and updates its state to Running.
    static void contextSwitch(Ptr<Proc> const& curr, Ptr<Proc> const& to);

protected:
    // Create a process. The process starts in the blocked state.
    // @param id: The unique identifier of the process.
    // @param addrSpace: The address space of the process.
    // @param kernelStack: The kernel stack to be used by this process.
    Proc(Id const id,
         Ptr<Paging::AddrSpace> const& addrSpace,
         Ptr<Memory::Stack> const& kernelStack);

    // Needed to invoke the constructor.
    friend Ptr<Proc>;
    // Needed to access the internal state of the process.
    friend SelfTests::TestResult procCreationAndJumpTest();

    // The unique identifier of this process.
    Id m_id;
    // The address space of this process.
    Ptr<Paging::AddrSpace> m_addrSpace;
    // The kernel stack used by the process.
    Ptr<Memory::Stack> m_kernelStack;
    // The saved RSP of this process during the last context switch.
    // FIXME: For now this must be a raw u64 and not a VirAddr. This is because
    // contextSwitch only accepts a u64* for the saving location.
    u64 m_savedKernelStackPointer;
    // Current state of the process.
    State m_state;
};

}
