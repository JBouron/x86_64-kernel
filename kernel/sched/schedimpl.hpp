// Scheduling related implementation details, not to be imported from outside
// kernel/sched/*.
#pragma once

#include <util/ints.hpp>

namespace Sched {

// Perform a context switch to another stack. This function saves the
// callee-saved registers, including the current stack pointer and changes the
// RSP to point to the new stack.
// @param newRsp: The stack to switch to.
// @param rspSave: Where to save the value of the current RSP/stack after
// pushing the callee-saved registers. This is used so that we may switch _back_
// to the current context using this saved value.
extern "C" void contextSwitch(u64 const newRsp, u64* const rspSave);
}
