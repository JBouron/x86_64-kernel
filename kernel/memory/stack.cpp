#include <memory/stack.hpp>
#include <framealloc/framealloc.hpp>
#include <smp/smp.hpp>
#include <util/assert.hpp>

namespace Stack {

// FIXME: We should only have one such const in the entire code base.
static constexpr u64 PAGE_SIZE = 0x1000;

// FIXME: For now the allocation is pretty naive: just allocate stacks one after
// the other at the very top of the virtual address space. This is fine for now
// as we are never free'ing stacks: those are used by the CPUs. Eventually, once
// we have task switching implemented, we might want an actual allocator capable
// of freeing stacks.
// The next virtual address where a stack will be allocated. Allocations work in
// "reverse", ie. going down.
static VirAddr nextStackAllocationAddr(-PAGE_SIZE);

// The default size of allocated stacks in number of pages.
// 16KiB by default.
static constexpr u64 DEFAULT_STACK_PAGES = 4;

// Special function to catch cpus that may attempt to return after switching to
// a new stack. allocate() craft a special stack frame at the bottom of
// allocated stacks to "return" to this function instead of returning to an
// arbitrary address.
static void limbo() {
    PANIC("Cpu {} attempted a return on an empty stack", Smp::id());
}

// Allocate a new stack in kernel virtual memory to be used by a CPU.
// @return: The virtual address of the _top_ of the allocated stack.
Res<VirAddr> allocate() {
    VirAddr const stackTop(nextStackAllocationAddr);
    VirAddr const stackBot(stackTop - DEFAULT_STACK_PAGES * PAGE_SIZE);
    ASSERT(stackTop.isPageAligned());
    ASSERT(stackBot.isPageAligned());

    nextStackAllocationAddr = stackBot;

    // Allocate physical frames for the stack an map them to the vir addr.
    Paging::PageAttr const attr(Paging::PageAttr::Writable);
    for (u64 i(0); i < DEFAULT_STACK_PAGES; ++i) {
        Res<Frame> const allocRes(FrameAlloc::alloc());
        if (!allocRes) {
            return allocRes.error();
        }

        Err const mapErr(Paging::map(stackBot + i * PAGE_SIZE,
                                     allocRes.value().addr(),
                                     attr,
                                     1));
        if (mapErr) {
            return mapErr.error();
        }
    }
    Log::debug("Allocated {} bytes stack @{}", DEFAULT_STACK_PAGES * PAGE_SIZE,
        stackBot);

    // Craft a special stack frame at the top of the stack so that if the cpu
    // using this stack ever returns when it is not supposed to, it jumps to
    // limbo() instead of a random address.
    // Return address.
    *((stackTop - 8).ptr<void(*)()>()) = limbo;
    return stackTop - 8;
}

// Change the stack pointer to the new top and jump to the given location. This
// function does not return.
// Implemented in assembly.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
extern "C" void _switchToStackAndJumpTo(u64 const newStackTop,
                                        u64 const jmpTarget);

// Change the stack pointer to the new top and jump to the given location. This
// function does NOT return.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
void switchToStack(VirAddr const newStackTop, void (*jmpTarget)()) {
    Log::debug("Cpu {} switching to stack @{}", Smp::id(), newStackTop);
    _switchToStackAndJumpTo(newStackTop.raw(),
                            reinterpret_cast<u64>(jmpTarget));
}
}
