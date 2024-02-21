#include <memory/stack.hpp>
#include <framealloc/framealloc.hpp>
#include <smp/smp.hpp>
#include <util/assert.hpp>
#include <logging/log.hpp>
#include <util/panic.hpp>
#include <datastruct/freelist.hpp>

namespace Memory {

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
    // Starting at -0x1000 is required because Stack::m_high is inclusive and we
    // cannot represent address 0x10000000000000000.
    VirAddr m_arenaStart = -0x1000;

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

static Allocator StackAllocator;
static Concurrency::SpinLock StackAllocatorLock;

// Allocate a new stack in kernel virtual memory to be used by a CPU.
// @return: The low address of the stack.
static Res<VirAddr> allocate() {
    Concurrency::LockGuard guard(StackAllocatorLock);
    Res<VirAddr> const allocRes(StackAllocator.alloc());
    if (!allocRes) {
        return allocRes.error();
    }

    u64 const stackSize(DEFAULT_STACK_PAGES * PAGE_SIZE);
    VirAddr const stackTop(allocRes.value() + stackSize);
    VirAddr const stackBot(allocRes.value());
    Log::debug("Allocated stack {}-{}", stackBot, stackTop);
    return stackBot;
}

// De-allocate a stack in kernel virtual memory.
// @param stack: The stack to de-allocate. This should be the low address of the
// stack, ie the address that was returned by the allocate call.
static void free(VirAddr const stack) {
    Concurrency::LockGuard guard(StackAllocatorLock);
    StackAllocator.free(stack);
    u64 const stackSize(DEFAULT_STACK_PAGES * PAGE_SIZE);
    VirAddr const stackTop(stack + stackSize);
    VirAddr const stackBot(stack);
    Log::debug("De-allocated stack {}-{}", stackBot, stackTop);
}

// Allocate a new stack in memory.
// @return: A pointer to the Stack instance associated with the allocated
// stack or an error, if any.
Res<Ptr<Stack>> Stack::New() {
    Res<VirAddr> const allocRes(allocate());
    if (!allocRes) {
        return allocRes.error();
    }

    // FIXME: For now, all stacks have the same hardcoded size.
    u64 const stackSize(DEFAULT_STACK_PAGES * PAGE_SIZE);
    VirAddr const low(allocRes.value());
    VirAddr const high(low + stackSize);

    // FIXME: We need to introduce a shortcut to perform those
    // if-alloc-ok-else-error schenanigans.
    Ptr<Stack> const stack(Ptr<Stack>::New(low, high));
    if (!stack) {
        return Error::MaxHeapSizeReached;
    } else {
        return stack;
    }
}

// De-allocate the associated memory upon destruction.
Stack::~Stack() {
    free(m_low);
}

// Get the high address of this stack.
VirAddr Stack::highAddress() const {
    return m_high;
}

// Create a Stack instance.
// @param low: Low address of the stack.
// @param high: High address of the stack.
Stack::Stack(VirAddr const low, VirAddr const high) : m_low(low), m_high(high) {
    ASSERT(low < high);
    ASSERT(low.isPageAligned());
    ASSERT(high.isPageAligned());
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
