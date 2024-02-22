// Process representation and manipulation.
#pragma once
#include <util/ints.hpp>
#include <util/addr.hpp>
#include <memory/stack.hpp>
#include <selftests/selftests.hpp>

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

    // Jump to the context of the given process. This function does not save the
    // current context and does not return! This is only meant to be used when
    // switching from the boot satck/context to the very first process running
    // on a cpu since at that point there is no Proc associated with the current
    // context.
    // @param to: The process to switch to.
    static void jumpToContext(Ptr<Proc> const& to);

    // Switch from the current context to another process' context.
    // @param curr: The Proc associated with the current context. This function
    // saves the current context in this Proc.
    // @param to: The process to switch execution to.
    static void contextSwitch(Ptr<Proc> const& curr, Ptr<Proc> const& to);

protected:
    // Create a process.
    // @param id: The unique identifier of the process.
    // @param kernelStack: The kernel stack to be used by this process.
    Proc(Id const id, Ptr<Memory::Stack> const kernelStack);

    // Needed to invoke the constructor.
    friend Ptr<Proc>;
    // Needed to access the internal state of the process.
    friend SelfTests::TestResult procCreationAndJumpTest();

    // The unique identifier of this process.
    Id m_id;
    // The kernel stack used by the process.
    Ptr<Memory::Stack> m_kernelStack;
    // The saved RSP of this process during the last context switch.
    // FIXME: For now this must be a raw u64 and not a VirAddr. This is because
    // contextSwitch only accepts a u64* for the saving location.
    u64 m_savedKernelStackPointer;
};

}
