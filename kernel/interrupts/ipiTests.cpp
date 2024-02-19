// IPI tests.
#include <interrupts/ipi.hpp>
#include <selftests/macros.hpp>
#include <interrupts/vectormap.hpp>

namespace Interrupts::Ipi {

// Flags used by the sendIpi test.
static Smp::Id sendIpiTestRemoteCpuId;
static volatile bool sendIpiTestFlag;

static void sendIpiTestIntHandler(__attribute__((unused)) Vector const v,
                                  __attribute__((unused))Frame const& frame) {
    Log::debug("sendIpiTestIntHandler running on cpu {}", Smp::id());
    sendIpiTestRemoteCpuId = Smp::id();
    sendIpiTestFlag = true;
}

SelfTests::TestResult sendIpiTest() {
    TEST_REQUIRES_MULTICORE();
    Vector const ipiVec(Interrupts::VectorMap::TestVector);
    TemporaryInterruptHandlerGuard guard(ipiVec, sendIpiTestIntHandler);

    // Send an IPI to each AP. Wait for the AP to set the sendIpiTestRemoteCpuId
    // and sendIpiTestFlag.
    for (Smp::Id id(0); id < Smp::ncpus(); ++id) {
        sendIpiTestRemoteCpuId = Smp::Id(0xff);
        sendIpiTestFlag = false;
        sendIpi(id, ipiVec);
        TEST_WAIT_FOR(!!sendIpiTestFlag, 5000);
        TEST_ASSERT(sendIpiTestRemoteCpuId == id);
    }
    return SelfTests::TestResult::Success;
}

// Run the IPI tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, sendIpiTest);
}
}
