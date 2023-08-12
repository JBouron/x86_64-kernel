// Interrupt related tests.
#include <interrupts/interrupts.hpp>

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
    auto const clobberingHandler([](__attribute__((unused)) Vector const v) {
        clobberCallerSavedRegisters();
    });
    Interrupts::registerHandler(1, clobberingHandler);
    TEST_ASSERT(interruptRegistersSavedTestRun());
    Interrupts::deregisterHandler(1);
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

    gotVector = 0;
    gotInterrupt = false;

    // A simple interrupt handler that sets the gotVector var to the interrupt
    // it received and sets the gotInterrupt flag to true.
    auto const testHandler([](Vector const v) {
        gotVector = v;
        gotInterrupt = true;
    });

    Interrupts::registerHandler(testVector, testHandler);

    // Raise interrupt. Unfortunately the value of the interrupt is hardcoded
    // here.
    asm("int $1");

    // The testHandler should have been called and set the gotInterrupt to 1.
    TEST_ASSERT(gotInterrupt);
    TEST_ASSERT(gotVector == testVector);

    // Deregister the handler. Unfortunately we don't really have a way to test
    // that the handler is not called after this.
    Interrupts::deregisterHandler(testVector);
    return SelfTests::TestResult::Success;
}

// Run the interrupt tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, interruptTest);
    RUN_TEST(runner, interruptRegistersSavedTest);
    RUN_TEST(runner, interruptHandlerRegistrationTest);
}

}
