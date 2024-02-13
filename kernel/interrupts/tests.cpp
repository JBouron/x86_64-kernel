// Interrupt related tests.
#include <interrupts/interrupts.hpp>
#include <interrupts/lapic.hpp>
#include <selftests/macros.hpp>
#include "ioapic.hpp"

namespace Interrupts {

// A few interrupt handlers for the interruptTest functions. Those handlers are
// written in assembly and simply set the flag defined below to the value of the
// interrupt vector they are associated with.
extern "C" void interruptTestHandler0();
extern "C" void interruptTestHandler1();
extern "C" void interruptTestHandler3();

// The flag written by the interruptTestHandlerX function from assembly. Needs C
// linkage.
extern "C" {
    u64 interruptTestFlag;
}

// Simple test where we raise an interrupt and verify that the correct handler
// has been called.
SelfTests::TestResult interruptTest() {
    // Save the current IDTR to be restored later.
    Cpu::TableDesc const origIdt(Cpu::sidt());

    Cpu::SegmentSel const codeSel(Cpu::readSegmentReg(Cpu::SegmentReg::Cs));

    // The IDT to be used in this test.
    static Interrupts::Descriptor testIdt[] = {
        Interrupts::Descriptor(codeSel,
                               reinterpret_cast<u64>(interruptTestHandler0),
                               Cpu::PrivLevel::Ring0,
                               Interrupts::Descriptor::Type::InterruptGate),
        Interrupts::Descriptor(codeSel,
                               reinterpret_cast<u64>(interruptTestHandler1),
                               Cpu::PrivLevel::Ring0,
                               Interrupts::Descriptor::Type::InterruptGate),
        // Avoid interrupt vector #NMI.
        Interrupts::Descriptor(),
        Interrupts::Descriptor(codeSel,
                               reinterpret_cast<u64>(interruptTestHandler3),
                               Cpu::PrivLevel::Ring0,
                               Interrupts::Descriptor::Type::InterruptGate),
    };

    // Load the test IDT.
    u64 const base(reinterpret_cast<u64>(testIdt));
    u16 const limit(sizeof(testIdt) - 1);
    Cpu::TableDesc const testIdtDesc(base, limit);
    Cpu::lidt(testIdtDesc);

    // Raise interrupt vectors 0 through 2. For each vector, the handler is
    // setting the value of interruptTestFlag to the value of the vector.
    // Reset the interruptTestFlag between each interrupt.

    interruptTestFlag = ~0ULL;
    asm("int $0");
    TEST_ASSERT(interruptTestFlag == 0);

    interruptTestFlag = ~0ULL;
    asm("int $1");
    TEST_ASSERT(interruptTestFlag == 1);

    interruptTestFlag = ~0ULL;
    asm("int $3");
    TEST_ASSERT(interruptTestFlag == 3);

    Cpu::lidt(origIdt);
    return SelfTests::TestResult::Success;
}

// Clobber all caller-saved registers. Implemented in interruptTestAsm.asm.
extern "C" void clobberCallerSavedRegisters();

// Implemented in assembly, this is the core of the
// interruptHandlerRegistrationTest. This routine triggers a software interrupt
// of vector = 1 and then asserts that all registers have their unchanged during
// the interrupt.
// @return: true if the value of all registers were unchanged by the interrupt,
// false otherwise.
extern "C" bool interruptRegistersSavedTestRun();

// Check that the kernel's interrupt handler does not clobber the registers of
// the interrupted context.
SelfTests::TestResult interruptRegistersSavedTest() {
    // A dumb interrupt handler that clobbers all caller-saved registers.
    auto const clobberingHandler([](__attribute__((unused)) Vector const v,
                                    __attribute__((unused)) Frame const& f) {
        clobberCallerSavedRegisters();
    });
    TemporaryInterruptHandlerGuard guard(Vector(1), clobberingHandler);
    TEST_ASSERT(interruptRegistersSavedTestRun());
    return SelfTests::TestResult::Success;
}

// Test the registering of interrupt handlers.
SelfTests::TestResult interruptHandlerRegistrationTest() {
    // The vector on which we are running this test. This vector must not
    // normally generate an error code as we are using a software interrupt in
    // this test.
    Vector const testVector(1);
    static Vector gotVector;
    static bool gotInterrupt;

    gotVector = Vector(0);
    gotInterrupt = false;

    // A simple interrupt handler that sets the gotVector var to the interrupt
    // it received and sets the gotInterrupt flag to true.
    auto const testHandler([](Vector const v,
                              __attribute__((unused)) Frame const& frame) {
        gotVector = v;
        gotInterrupt = true;
    });

    TemporaryInterruptHandlerGuard guard(testVector, testHandler);

    // Raise interrupt. Unfortunately the value of the interrupt is hardcoded
    // here.
    asm("int $1");

    // The testHandler should have been called and set the gotInterrupt to 1.
    TEST_ASSERT(gotInterrupt);
    TEST_ASSERT(gotVector == testVector);
    return SelfTests::TestResult::Success;
}

// Set all registers to their expected values (defined below) and raise a
// software interrupt with vector 0x1.
extern "C" void interruptHandlerFrameTestRun();

extern "C" u64 const interruptHandlerFrameTestExpectedRip;
extern "C" u64 const interruptHandlerFrameTestExpectedRax;
extern "C" u64 const interruptHandlerFrameTestExpectedRbx;
extern "C" u64 const interruptHandlerFrameTestExpectedRcx;
extern "C" u64 const interruptHandlerFrameTestExpectedRdx;
extern "C" u64 const interruptHandlerFrameTestExpectedRsi;
extern "C" u64 const interruptHandlerFrameTestExpectedRdi;
extern "C" u64 const interruptHandlerFrameTestExpectedRbp;
extern "C" u64 const interruptHandlerFrameTestExpectedR8;
extern "C" u64 const interruptHandlerFrameTestExpectedR9;
extern "C" u64 const interruptHandlerFrameTestExpectedR10;
extern "C" u64 const interruptHandlerFrameTestExpectedR11;
extern "C" u64 const interruptHandlerFrameTestExpectedR12;
extern "C" u64 const interruptHandlerFrameTestExpectedR13;
extern "C" u64 const interruptHandlerFrameTestExpectedR14;
extern "C" u64 const interruptHandlerFrameTestExpectedR15;
extern "C" u64 const interruptHandlerFrameTestExpectedRflags;

// Test that the Frame struct passed to an interrupt handler contains the
// correct values for all registers.
SelfTests::TestResult interruptHandlerFrameTest() {
    // Make sure to use a vector that does not normally generate an error code.
    Vector const testVector(1);
    // Indicate if the interrupt handler received the interrupt at all.
    static bool gotInterrupt;
    gotInterrupt = false;

    auto const testHandler([](__attribute__((unused)) Vector const vector,
                              Frame const& frame) {
        gotInterrupt = true;
        // The assembly function defines what value is expected for each
        // register.
        // The error code is expected to be zero given that this vector does not
        // generate one (and we are using software interrupts anyway). An actual
        // test case for the error code can be found in the paging test.
        ASSERT(!frame.errorCode);
        ASSERT(frame.rip ==
            reinterpret_cast<u64>(&interruptHandlerFrameTestExpectedRip));
        ASSERT(frame.rax == interruptHandlerFrameTestExpectedRax);
        ASSERT(frame.rbx == interruptHandlerFrameTestExpectedRbx);
        ASSERT(frame.rcx == interruptHandlerFrameTestExpectedRcx);
        ASSERT(frame.rdx == interruptHandlerFrameTestExpectedRdx);
        ASSERT(frame.rsi == interruptHandlerFrameTestExpectedRsi);
        ASSERT(frame.rdi == interruptHandlerFrameTestExpectedRdi);
        ASSERT(frame.rbp == interruptHandlerFrameTestExpectedRbp);
        ASSERT(frame.r8  == interruptHandlerFrameTestExpectedR8);
        ASSERT(frame.r9  == interruptHandlerFrameTestExpectedR9);
        ASSERT(frame.r10 == interruptHandlerFrameTestExpectedR10);
        ASSERT(frame.r11 == interruptHandlerFrameTestExpectedR11);
        ASSERT(frame.r12 == interruptHandlerFrameTestExpectedR12);
        ASSERT(frame.r13 == interruptHandlerFrameTestExpectedR13);
        ASSERT(frame.r14 == interruptHandlerFrameTestExpectedR14);
        ASSERT(frame.r15 == interruptHandlerFrameTestExpectedR15);
        ASSERT(frame.rflags == interruptHandlerFrameTestExpectedRflags);
    });

    TemporaryInterruptHandlerGuard guard(testVector, testHandler);

    interruptHandlerFrameTestRun();
    TEST_ASSERT(gotInterrupt);
    return SelfTests::TestResult::Success;
}

// Run the interrupt tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, interruptTest);
    RUN_TEST(runner, interruptRegistersSavedTest);
    RUN_TEST(runner, interruptHandlerRegistrationTest);
    RUN_TEST(runner, interruptHandlerFrameTest);

    Lapic::Test(runner);
    IoApic::Test(runner);
}

}
