// Everything related to CPU stack allocation and managment.
#pragma once
#include <util/addr.hpp>
#include <util/result.hpp>

namespace Stack {

// Allocate a new stack in kernel virtual memory to be used by a CPU.
// @return: The virtual address of the _top_ of the allocated stack, or any
// error.
Res<VirAddr> allocate();

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
