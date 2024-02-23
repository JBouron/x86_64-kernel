// Process representation and manipulation.
#include <sched/process.hpp>
#include <memory/stack.hpp>
#include <smp/smp.hpp>
#include <util/panic.hpp>
#include <cpu/cpu.hpp>
#include "./schedimpl.hpp"

namespace Sched {
// Create a process.
// @param id: The unique identifier of the process.
// @return: A pointer to the Proc instance or an error, if any.
Res<Ptr<Proc>> Proc::New(Id const id) {
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

    Ptr<Proc> const proc(Ptr<Proc>::New(id, addrSpace, kernelStack));
    if (!proc) {
        return Error::MaxHeapSizeReached;
    } else {
        return proc;
    }
}

// Catch a process running a function that returned from that function. This
// raises a PANIC.
static void handleRetFromProc() {
    // FIXME: For now, we don't have a pointer on Proc object associated with
    // the current process, hence cannot print its ID.
    PANIC("Process returned from its function call");
}

// Create a process that executes a function or lambda.
// @param id: The unique identifier of the process.
// @param func: The function to be executed. This must be a function taking no
// argument. If this function ever returns, a PANIC is raised.
// @return: A pointer to the Proc instance or an error, if any.
Res<Ptr<Proc>> Proc::New(Id const id, void (*func)(void)) {
    Res<Ptr<Proc>> const procAlloc(Proc::New(id));
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
    return proc;
}

// Get the unique identifier associated with this process.
Proc::Id Proc::id() const {
    return m_id;
}

// Create a process.
// @param id: The unique identifier of the process.
// @param kernelStack: The kernel stack to be used by this process.
Proc::Proc(Id const id,
           Ptr<Paging::AddrSpace> const& addrSpace,
           Ptr<Memory::Stack> const& kernelStack) :
    m_id(id),
    m_addrSpace(addrSpace),
    m_kernelStack(kernelStack),
    m_savedKernelStackPointer(kernelStack->highAddress()) {}

// Jump to the context of the given process. This function does not save the
// current context and does not return! This is only meant to be used when
// switching from the boot satck/context to the very first process running on a
// cpu since at that point there is no Proc associated with the current context.
// @param to: The process to switch to.
void Proc::jumpToContext(Ptr<Proc> const& to) {
    // Save the current stack pointer on a dummy variable, we will _NOT_ return
    // to this stack ever.
    u64 dummy;
    Paging::AddrSpace::switchAddrSpace(to->m_addrSpace);
    Sched::contextSwitch(to->m_savedKernelStackPointer, &dummy);
}

// Switch from the current context to another process' context and address
// space.
// @param curr: The Proc associated with the current context. This function
// saves the current context in this Proc.
// @param to: The process to switch execution to.
void Proc::contextSwitch(Ptr<Proc> const& curr, Ptr<Proc> const& to) {
    Paging::AddrSpace::switchAddrSpace(to->m_addrSpace);
    Sched::contextSwitch(to->m_savedKernelStackPointer,
                         &curr->m_savedKernelStackPointer);
}
}
