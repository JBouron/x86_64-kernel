// Everything related to CPU stack allocation and managment.
#pragma once
#include <util/addr.hpp>
#include <util/result.hpp>
#include <util/ptr.hpp>

namespace Memory {

// Describes a stack that has been allocated in kernel virtual memory. This
// class uses RAII in the sense that the destructor automatically de-allocate
// the virtual memory used by this stack.
// A Stack instance has ownership of the memory it covers. As such the type is
// non-copyable and non-copy-assignable to avoid having multiple Stack instances
// referring to the same memory.
class Stack {
public:
    // Allocate a new stack in memory.
    // @return: A pointer to the Stack instance associated with the allocated
    // stack or an error, if any.
    static Res<Ptr<Stack>> New();

    // De-allocate the associated memory upon destruction.
    ~Stack();

    // Stacks are not copyable nor assignable. This is to avoid having two
    // instances claiming ownership of the same memory.
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;

    // For now the move constructor and assignment are also deleted as we don't
    // need them. This might change in the future.
    Stack(Stack&&) = delete;
    Stack& operator=(Stack&&) = delete;

    // Get the low or high address of this stack.
    VirAddr lowAddress() const;
    VirAddr highAddress() const;

private:
    // Create a Stack instance.
    // @param low: Low address of the stack.
    // @param high: High address of the stack.
    Stack(VirAddr const low, VirAddr const high);

    // Needed to access private constructor.
    friend Ptr<Stack>;

    // Lowest address contained in the stack.
    VirAddr m_low;
    // Highest address contained in the stack.
    VirAddr m_high;
};

// Change the stack pointer to the new top and jump to the given location. This
// function does NOT return.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
void switchToStack(VirAddr const newStackTop, void (*jmpTarget)());

// Change the stack pointer to the new top and jump to the given location. This
// function also allows passing an argument to the target function. This
// function does NOT return.
// @param newStackTop: Virtual address of the new stack to use.
// @param jmpTarget: Destination of the jump after switching to the new stack.
// @param arg: Argument to pass to the jmpTarget function pointer.
void switchToStack(VirAddr const newStackTop,
                   void (*jmpTarget)(u64),
                   u64 const arg);
}
