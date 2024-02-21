// Scheduling related tests.
#include <selftests/macros.hpp>
#include <sched/sched.hpp>
#include "schedimpl.hpp"
#include <memory/stack.hpp>

namespace Sched {

extern "C" {
// Saved RSP for the stack that executed contextSwitchTest, ie. the BSP's
// stack/context.
u64 contextSwitchTestSavedOrigRsp;
// Saved RSP for the new context running contextSwitchTestTarget.
u64 contextSwitchTestSavedContextRsp;
// Indicate if switching to the new context was successful.
bool contextSwitchTestTargetFlag;
}

// The code that is run in a new context during contextSwitchTest. This function
// performs the following:
//  1. Clobber all callee-saved registers.
//  2. Set the contextSwitchTestTargetFlag to 1.
//  3. Switching back to the previous context, ie. the context that is executing
//  the test.
// Written in assembly.
extern "C" void contextSwitchTestTarget();

// Checks that a context switch does not clobber callee-saved registers.
// Performs the following:
//  1. Set the callee-saved registers to some peculiar values.
//  2. Call contextSwitch with the given arguments.
//  3. Once contextSwitch returns, ie. contextSwitchTestTarget called
//  contextSwitch, check the value of the callee-saved registers.
//  4. If all values match the previous values, return true, else false.
// Written in assembly
extern "C" bool initiateContextSwitchAndCheckCalleeSavedRegs(u64 const newRsp,
                                                           u64* const savedRsp);

// Check that Sched::contextSwitch() is correct, that is it indeed switches to
// the new stack and it preserves the callee-saved registers.
SelfTests::TestResult contextSwitchTest() {
    // In this test we are going to have two contexts:
    //  - Context A is the context that is calling this (and all the other)
    //  test, ie the context running under kernelMain.
    //  - Context B is a new context that will simply run
    //  contextSwitchTestTarget before switching back to context A.
    contextSwitchTestTargetFlag = false;
    contextSwitchTestSavedOrigRsp = 0;
    contextSwitchTestSavedContextRsp = 0;

    // Allocate a stack for context B.
    Res<VirAddr> const allocRes(Stack::allocate());
    TEST_ASSERT(allocRes.ok());

    // Prepare a stack frame to "return" to contextSwitchTestTarget after the
    // context switch to context B.
    VirAddr const contextBStack(allocRes.value() + 8);
    // FIXME: Stack::allocate() return the address of the bottom of the
    // stack and also pushes a return address onto the new stack. This creates a
    // headache when free'ing the stack as we need to compute the address of the
    // top of the stack.
    VirAddr const contextBStackTop(contextBStack - 4 * 0x1000);
    u64* rsp(contextBStack.ptr<u64>());
    // Push the return address.
    rsp -= 1;
    *rsp = reinterpret_cast<u64>(&contextSwitchTestTarget);
    // Push the saved callee-saved registers. E.g. we are pretending that this
    // stack as ran contextSwitch before.
    for (u64 i(0); i < 6; ++i) {
        rsp -= 1;
        *rsp = 0;
    }

    // Switch to context B.
    Log::debug("Switching to context B");
    bool const savedRegsOk(initiateContextSwitchAndCheckCalleeSavedRegs(
        reinterpret_cast<u64>(rsp), &contextSwitchTestSavedOrigRsp));
    Log::debug("Returned from contextSwitch");
    // Check that we indeed performed a context switch.
    TEST_ASSERT(contextSwitchTestTargetFlag);
    // Check that the callee-saved registers were not changed.
    TEST_ASSERT(savedRegsOk);

    Stack::free(contextBStackTop);
    return SelfTests::TestResult::Success;
}

// Run scheduling related tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, contextSwitchTest);
}
}
