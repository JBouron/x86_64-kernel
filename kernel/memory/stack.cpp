#include <memory/stack.hpp>
#include <framealloc/framealloc.hpp>
#include <smp/smp.hpp>
#include <util/assert.hpp>
#include <logging/log.hpp>
#include <util/panic.hpp>
#include <datastruct/freelist.hpp>

namespace Stack {

// FIXME: We should only have one such const in the entire code base.
static constexpr u64 PAGE_SIZE = 0x1000;

// The default size of allocated stacks in number of pages.
// 16KiB by default.
static constexpr u64 DEFAULT_STACK_PAGES = 4;

// Stack allocator. This allocates stacks from the very top of the virtual
// address space.
class Allocator {
public:
    // Allocate a new stack.
    // @return: The virtual address of the allocated stack, or an error, if any.
    Res<VirAddr> alloc() {
        while (true) {
            u64 const allocSize(DEFAULT_STACK_PAGES * PAGE_SIZE);
            Res<VirAddr> const allocRes(m_freeList.alloc(allocSize));
            if (allocRes) {
                VirAddr const stack(allocRes.value());
                ASSERT(stack.isPageAligned());
                return stack;
            } else {
                // The allocation can only fail if there was no space available
                // in the EmbeddedFreeList. Grow the arena.
                Err const err(growArena(DEFAULT_STACK_PAGES * 4));
                if (err) {
                    return err.error();
                }
            }
        }
    }

    // Free an allocated stack.
    // @param stack: The stack to de-allocate.
    void free(VirAddr const stack) {
        ASSERT(stack.isPageAligned());
        m_freeList.free(stack, DEFAULT_STACK_PAGES * PAGE_SIZE);
    }

private:
    DataStruct::EmbeddedFreeList m_freeList;

    // The current start of the virtual memory area used for stack allocation.
    // We start at addres 0xffff...ffff + 1 = 0x0.
    VirAddr m_arenaStart;

    // Grow the virtual memory arena used to allocate stacks.
    // @param numPages: The number of pages to add to the arena.
    // @return: Any error that occured while growing the arena.
    Err growArena(u64 const numPages) {
        Paging::PageAttr const attr(Paging::PageAttr::Writable);
        for (u64 i(0); i < numPages; ++i) {
            Res<Frame> const allocRes(FrameAlloc::alloc());
            if (!allocRes) {
                return allocRes.error();
            }

            Err const mapErr(Paging::map(m_arenaStart - PAGE_SIZE,
                                         allocRes.value().addr(),
                                         attr,
                                         1));
            if (mapErr) {
                return mapErr.error();
            }
            m_arenaStart = m_arenaStart - PAGE_SIZE;
        }
        m_freeList.insert(m_arenaStart, numPages * PAGE_SIZE);
        return Ok;
    }
};

// Special function to catch cpus that may attempt to return after switching to
// a new stack. allocate() craft a special stack frame at the bottom of
// allocated stacks to "return" to this function instead of returning to an
// arbitrary address.
static void limbo() {
    PANIC("Cpu {} attempted a return on an empty stack", Smp::id());
}

static Allocator StackAllocator;
static Concurrency::SpinLock StackAllocatorLock;

// Allocate a new stack in kernel virtual memory to be used by a CPU.
// @return: The virtual address of the _top_ of the allocated stack.
Res<VirAddr> allocate() {
    Concurrency::LockGuard guard(StackAllocatorLock);
    Res<VirAddr> const allocRes(StackAllocator.alloc());
    if (!allocRes) {
        return allocRes.error();
    }

    u64 const stackSize(DEFAULT_STACK_PAGES * PAGE_SIZE);
    VirAddr const stackTop(allocRes.value() + stackSize);
    VirAddr const stackBot(allocRes.value());

    Log::debug("Allocated {} bytes stack {}-{}", stackSize, stackBot, stackTop);

    // Craft a special stack frame at the top of the stack so that if the cpu
    // using this stack ever returns when it is not supposed to, it jumps to
    // limbo() instead of a random address.
    // Return address.
    *((stackTop - 8).ptr<void(*)()>()) = limbo;
    return stackTop - 8;
}

// De-allocate a stack in kernel virtual memory.
// @param stack: The stack to de-allocate.
void free(VirAddr const stack) {
    Concurrency::LockGuard guard(StackAllocatorLock);
    StackAllocator.free(stack);
}

// Change the stack pointer to the new top and jump to the given location. This
// function does not return.
// Implemented in assembly.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
// @param arg: Argument to pass to the jmpTarget function pointer.
extern "C" void _switchToStackAndJumpTo(u64 const newStackTop,
                                        u64 const jmpTarget,
                                        u64 const arg);

// Change the stack pointer to the new top and jump to the given location. This
// function does NOT return.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
// @param arg: Argument to pass to the jmpTarget function pointer.
void switchToStack(VirAddr const newStackTop, void (*jmpTarget)()) {
    Log::debug("Cpu {} switching to stack @{}", Smp::id(), newStackTop);
    // Pass dummy argument.
    _switchToStackAndJumpTo(newStackTop.raw(),
                            reinterpret_cast<u64>(jmpTarget), 0);
}

// Change the stack pointer to the new top and jump to the given location. This
// function does NOT return.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
// @param arg: Argument to pass to the jmpTarget function pointer.
void switchToStack(VirAddr const newStackTop,
                   void (*jmpTarget)(u64),
                   u64 const arg) {
    Log::debug("Cpu {} switching to stack @{}", Smp::id(), newStackTop);
    _switchToStackAndJumpTo(newStackTop.raw(),
                            reinterpret_cast<u64>(jmpTarget),
                            arg);
}
}
