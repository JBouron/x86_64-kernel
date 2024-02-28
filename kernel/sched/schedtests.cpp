// Scheduling related tests.
#include <selftests/macros.hpp>
#include <sched/sched.hpp>
#include "schedimpl.hpp"
#include <memory/stack.hpp>
#include <sched/process.hpp>
#include <smp/remotecall.hpp>

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
    Res<Ptr<Memory::Stack>> const allocRes(Memory::Stack::New());
    TEST_ASSERT(allocRes.ok());
    Ptr<Memory::Stack> const stack(allocRes.value());

    // Prepare a stack frame to "return" to contextSwitchTestTarget after the
    // context switch to context B.
    VirAddr const contextBStack(stack->highAddress());
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

    return SelfTests::TestResult::Success;
}

// Test creating and switching to a process.
SelfTests::TestResult procCreationAndJumpTest() {
    TEST_REQUIRES_MULTICORE();

    // Create a process an makes an AP jump to it. Assert that the code was
    // executed from the context of the process.
    // Because jumpToContext() does not return we have to test on a remote cpu
    // instead of the current one. We can always reset the remote cpu
    // afterwards.

    // Set to true in procFunc, indicating that it executed.
    static bool procFuncExecutedFlag;
    // Set to the value of the RSP register at the beginning of procFunc. Used
    // to assert that the stack indeed switched.
    static u64 procFuncRsp;
    // Set to the address of the loaded PML4 at the beginning of procFunc. Used
    // to assert that the address space indeed switched.
    static u64 procFuncPml4;
    // The code executed by the test process. Set the procFuncRsp and
    // procFuncExecutedFlag and halt forever.
    // The function cannot return, hence put the cpu into an infinite halt
    // loop. It will be reset anyway.
    auto const procFunc([]() {
        Log::debug("Running process context on cpu {}", Smp::id());
        procFuncRsp = Cpu::getRsp();
        Log::debug("RSP = {x}", procFuncRsp);
        procFuncPml4 = Cpu::cr3() & ~(PAGE_SIZE - 1);
        Log::debug("PML4 = {x}", procFuncPml4);
        procFuncExecutedFlag = true;
        while (true) {
            asm("hlt");
        }
    });
    procFuncRsp = 0;
    procFuncPml4 = 0;
    procFuncExecutedFlag = false;

    // Proc is static so that destCpu can access it further below.
    static Ptr<Proc> proc;
    proc = Proc::New(123, procFunc).value();
    TEST_ASSERT(proc->id() == 123);

    // The CPU that will run the process.
    Smp::Id const destCpu((Smp::id().raw() + 1) % Smp::ncpus());

    // Make the remote cpu jump to the new process' context.
    // Note: Instead of using a RemoteCall, we use startupApplicationProcessor
    // to avoid a memory leak. This is because the destCpu will never return
    // from this function and therefore will never have a chance to free the
    // CallDesc associated with the remote call.
    auto const runOnRemoteCpu([]() {
        Log::debug("Cpu {} jumping to proc {}", Smp::id(), proc->id());
        Proc::jumpToContext(proc);
        UNREACHABLE
    });
    Smp::startupApplicationProcessor(destCpu, runOnRemoteCpu);

    TEST_WAIT_FOR(procFuncExecutedFlag, 2000);

    // Check that the stack pointer when running in the process context was
    // within the process' kernel stack.
    // FIXME: Phy/VirAddr cannot be directly compared with u64s.
    Ptr<Memory::Stack> const procStack(proc->m_kernelStack);
    TEST_ASSERT(procStack->lowAddress() <= procFuncRsp
                && procFuncRsp < procStack->highAddress().raw());
    // Check that the address space changed when switching to the process.
    TEST_ASSERT(procFuncPml4 == proc->m_addrSpace->pml4Address().raw());
    // As a sanity check, we also check that the stack pointer is not contained
    // in the remote cpu's boot stack.
    Ptr<Memory::Stack> const destCpuStack(
        Smp::PerCpu::data(destCpu).kernelStack);
    TEST_ASSERT(!(destCpuStack->lowAddress() <= procFuncRsp
                && procFuncRsp < destCpuStack->highAddress().raw()));

    // Reset the remote cpu.
    Smp::startupApplicationProcessor(destCpu);

    // We need to manually "free" proc.
    // Sanity check that the proc is going to be freed and that we are not just
    // creating a leak.
    ASSERT(proc.refCount() == 1);
    proc = Ptr<Proc>();
    return SelfTests::TestResult::Success;
}

// Test context switching between different processes.
SelfTests::TestResult procContextSwitchTest() {
    TEST_REQUIRES_MULTICORE();

    // In this test we have three processes P1, P2 and P3. Both are executing
    // the same code:
    //  FOR i in 1...numItePerProc:
    //      INSERT id in vector VEC
    //      SWITCH to the next process
    // Once both processes are done, we check the content of VEC, it should be
    // a sequence 1, 2, 3, 1, 2, 3, ...

    static const u64 numItePerProc(10);
    static Vector<u64> IdVec;
    static Ptr<Proc> Proc1;
    static Ptr<Proc> Proc2;
    static Ptr<Proc> Proc3;
    static bool Proc1Done;
    static bool Proc2Done;
    static bool Proc3Done;

    // Duplicating the code here for two reasons:
    //  - We can't have a pointer to the "current" process, although this could
    //  be solved by adding a static that is updated before every call to
    //  contextSwitch.
    //  - This makes sure that we are doing the process creation and switch
    //  right, from the point of view of Proc, those three functions are
    //  completely different and at different addresses.
    auto const proc1Code([]() {
        for (u64 i(0); i < numItePerProc; ++i) {
            ASSERT(Proc1->state() == Proc::State::Running);
            ASSERT(Proc2->state() == Proc::State::Ready);
            ASSERT(Proc3->state() == Proc::State::Ready);
            IdVec.pushBack(1);
            // Cannot write true in the flag after the loop as we might not
            // return from the contextSwitch during the last iteration depending
            // on which process started first.
            Proc1Done = (i == numItePerProc - 1);
            Proc::contextSwitch(Proc1, Proc2);
        }
        while (true) { asm("hlt"); }
    });
    auto const proc2Code([]() {
        for (u64 i(0); i < numItePerProc; ++i) {
            ASSERT(Proc1->state() == Proc::State::Ready);
            ASSERT(Proc2->state() == Proc::State::Running);
            ASSERT(Proc3->state() == Proc::State::Ready);
            IdVec.pushBack(2);
            // Cannot write true in the flag after the loop as we might not
            // return from the contextSwitch during the last iteration depending
            // on which process started first.
            Proc2Done = (i == numItePerProc - 1);
            Proc::contextSwitch(Proc2, Proc3);
        }
        while (true) { asm("hlt"); }
    });
    auto const proc3Code([]() {
        for (u64 i(0); i < numItePerProc; ++i) {
            ASSERT(Proc1->state() == Proc::State::Ready);
            ASSERT(Proc2->state() == Proc::State::Ready);
            ASSERT(Proc3->state() == Proc::State::Running);
            IdVec.pushBack(3);
            // Cannot write true in the flag after the loop as we might not
            // return from the contextSwitch during the last iteration depending
            // on which process started first.
            Proc3Done = (i == numItePerProc - 1);
            Proc::contextSwitch(Proc3, Proc1);
        }
        while (true) { asm("hlt"); }
    });

    IdVec.clear();
    Proc1 = Proc::New(1, proc1Code).value();
    Proc2 = Proc::New(2, proc2Code).value();
    Proc3 = Proc::New(3, proc3Code).value();
    TEST_ASSERT(Proc1->state() == Proc::State::Ready);
    TEST_ASSERT(Proc2->state() == Proc::State::Ready);
    TEST_ASSERT(Proc3->state() == Proc::State::Ready);
    Proc1Done = false;
    Proc2Done = false;
    Proc3Done = false;

    // As in procCreationAndJumpTest, we use startupApplicationProcessor to
    // avoid memory leaks due to RemoteCall::CallDescs never being freed.
    Smp::Id const destCpu((Smp::id().raw() + 1) % Smp::ncpus());
    auto const runOnRemote([]() {
        Log::debug("Cpu {} switching to process 1", Smp::id());
        Proc::jumpToContext(Proc1);
        UNREACHABLE;
    });
    Smp::startupApplicationProcessor(destCpu, runOnRemote);

    TEST_WAIT_FOR(Proc1Done, 1000);
    TEST_WAIT_FOR(Proc2Done, 1000);
    TEST_WAIT_FOR(Proc3Done, 1000);

    // Check the content of the vector.
    TEST_ASSERT(IdVec.size() == 3 * numItePerProc);
    for (u64 i(0); i < IdVec.size(); ++i) {
        TEST_ASSERT(IdVec[i] == 1 + (i % 3));
    }

    // Reset the remote cpu.
    Smp::startupApplicationProcessor(destCpu);

    // We need to manually "free" the procs.
    // Sanity check that the proc is going to be freed and that we are not just
    // creating a leak.
    ASSERT(Proc1.refCount() == 1);
    ASSERT(Proc2.refCount() == 1);
    ASSERT(Proc3.refCount() == 1);
    Proc1 = Ptr<Proc>();
    Proc2 = Ptr<Proc>();
    Proc3 = Ptr<Proc>();

    return SelfTests::TestResult::Success;
}

// Run scheduling related tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, contextSwitchTest);
    RUN_TEST(runner, procCreationAndJumpTest);
    RUN_TEST(runner, procContextSwitchTest);
}
}
