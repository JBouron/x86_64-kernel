// Tests for the LocalApic.
#include "apic.hpp"
#include <framealloc/framealloc.hpp>
#include <util/assert.hpp>

namespace Interrupts {

// RAII-style class that creates a mock apic with an allocated backend page
// frame. The frame is free'ed when this class is destructed.
class MockLapicGuard {
public:
    MockLapicGuard() {
        Res<FrameAlloc::Frame> const alloc(FrameAlloc::alloc());
        ASSERT(alloc.ok());
        m_base = alloc->phyOffset();
        lapic = new LocalApic(m_base);
    }

    ~MockLapicGuard() {
        delete lapic;
        FrameAlloc::free(FrameAlloc::Frame(m_base.raw()));
        // FIXME: Revert the changes made to the mapping of the frame.
    }

    PhyAddr m_base;
    LocalApic* lapic;
};

// Check that the Lapic accesses the correct registers in readRegister() and
// writeRegister(). Incidentally also check that the Register enum is correctly
// defined.
SelfTests::TestResult localApicRegisterReadWriteTest() {
    MockLapicGuard guard;
    LocalApic* const lapic(guard.lapic);
    // Check that the Register enum is correctly defined.
    using Reg = LocalApic::Register;
    TEST_ASSERT(static_cast<u16>(Reg::ApicId                        ) == 0x020);
    TEST_ASSERT(static_cast<u16>(Reg::ApicVersion                   ) == 0x030);
    TEST_ASSERT(static_cast<u16>(Reg::TaskPriority                  ) == 0x080);
    TEST_ASSERT(static_cast<u16>(Reg::ArbitrationPriority           ) == 0x090);
    TEST_ASSERT(static_cast<u16>(Reg::ProcessorPriority             ) == 0x0a0);
    TEST_ASSERT(static_cast<u16>(Reg::EndOfInterrupt                ) == 0x0b0);
    TEST_ASSERT(static_cast<u16>(Reg::RemoteRead                    ) == 0x0c0);
    TEST_ASSERT(static_cast<u16>(Reg::LogicalDestination            ) == 0x0d0);
    TEST_ASSERT(static_cast<u16>(Reg::DestinationFormat             ) == 0x0e0);
    TEST_ASSERT(static_cast<u16>(Reg::SpuriousInterruptVector       ) == 0x0f0);
    TEST_ASSERT(static_cast<u16>(Reg::InService31to0                ) == 0x100);
    TEST_ASSERT(static_cast<u16>(Reg::InService63to32               ) == 0x110);
    TEST_ASSERT(static_cast<u16>(Reg::InService95to64               ) == 0x120);
    TEST_ASSERT(static_cast<u16>(Reg::InService127to96              ) == 0x130);
    TEST_ASSERT(static_cast<u16>(Reg::InService159to128             ) == 0x140);
    TEST_ASSERT(static_cast<u16>(Reg::InService191to160             ) == 0x150);
    TEST_ASSERT(static_cast<u16>(Reg::InService223to192             ) == 0x160);
    TEST_ASSERT(static_cast<u16>(Reg::InService255to224             ) == 0x170);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode31to0              ) == 0x180);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode63to32             ) == 0x190);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode95to64             ) == 0x1a0);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode127to96            ) == 0x1b0);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode159to128           ) == 0x1c0);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode191to160           ) == 0x1d0);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode223to192           ) == 0x1e0);
    TEST_ASSERT(static_cast<u16>(Reg::TriggerMode255to224           ) == 0x1f0);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest31to0         ) == 0x200);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest63to32        ) == 0x210);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest95to64        ) == 0x220);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest127to96       ) == 0x230);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest159to128      ) == 0x240);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest191to160      ) == 0x250);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest223to192      ) == 0x260);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptRequest255to224      ) == 0x270);
    TEST_ASSERT(static_cast<u16>(Reg::ErrorStatus                   ) == 0x280);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptCommandLow           ) == 0x300);
    TEST_ASSERT(static_cast<u16>(Reg::InterruptCommandHigh          ) == 0x310);
    TEST_ASSERT(static_cast<u16>(Reg::TimerLocalVectorTableEntry    ) == 0x320);
    TEST_ASSERT(static_cast<u16>(Reg::ThermalLocalVectorTableEntry  ) == 0x330);
    TEST_ASSERT(static_cast<u16>(Reg::PerformanceCounterLocalVectorTableEntry)
                == 0x340);
    TEST_ASSERT(static_cast<u16>(Reg::LocalInterrupt0VectorTableEntry)== 0x350);
    TEST_ASSERT(static_cast<u16>(Reg::LocalInterrupt1VectorTableEntry)== 0x360);
    TEST_ASSERT(static_cast<u16>(Reg::ErrorVectorTableEntry         ) == 0x370);
    TEST_ASSERT(static_cast<u16>(Reg::TimerInitialCount             ) == 0x380);
    TEST_ASSERT(static_cast<u16>(Reg::TimerCurrentCount             ) == 0x390);
    TEST_ASSERT(static_cast<u16>(Reg::TimerDivideConfiguration      ) == 0x3e0);

    // Now try to write a register, check its value in the register space and
    // read it again. Since we are not operating with a real Local Apic we can
    // write arbitrary values without bothering with reserved bits.
    Reg const registers[] = {
        Reg::ApicId, Reg::ApicVersion, Reg::TaskPriority,
        Reg::ArbitrationPriority, Reg::ProcessorPriority, Reg::EndOfInterrupt,
        Reg::RemoteRead, Reg::LogicalDestination, Reg::DestinationFormat,
        Reg::SpuriousInterruptVector, Reg::InService31to0, Reg::InService63to32,
        Reg::InService95to64, Reg::InService127to96, Reg::InService159to128,
        Reg::InService191to160, Reg::InService223to192, Reg::InService255to224,
        Reg::TriggerMode31to0, Reg::TriggerMode63to32, Reg::TriggerMode95to64,
        Reg::TriggerMode127to96, Reg::TriggerMode159to128,
        Reg::TriggerMode191to160, Reg::TriggerMode223to192,
        Reg::TriggerMode255to224, Reg::InterruptRequest31to0,
        Reg::InterruptRequest63to32, Reg::InterruptRequest95to64,
        Reg::InterruptRequest127to96, Reg::InterruptRequest159to128,
        Reg::InterruptRequest191to160, Reg::InterruptRequest223to192,
        Reg::InterruptRequest255to224, Reg::ErrorStatus,
        Reg::InterruptCommandLow, Reg::InterruptCommandHigh,
        Reg::TimerLocalVectorTableEntry, Reg::ThermalLocalVectorTableEntry,
        Reg::PerformanceCounterLocalVectorTableEntry,
        Reg::LocalInterrupt0VectorTableEntry,
        Reg::LocalInterrupt1VectorTableEntry, Reg::ErrorVectorTableEntry,
        Reg::TimerInitialCount, Reg::TimerCurrentCount,
        Reg::TimerDivideConfiguration,
    };
    for (Reg const& reg : registers) {
        if (reg == Reg::EndOfInterrupt) {
            // EOI cannot be read.
            continue;
        }
        // Set the register to some "random" value;
        VirAddr const baseVAddr(Paging::toVirAddr(lapic->m_base));
        u32 * const regPtr((baseVAddr + static_cast<u64>(reg)).ptr<u32>());
        // That should be random enough for our case.
        u32 const randValue(0xdeadbeef * static_cast<u64>(reg));
        *regPtr = randValue;

        // Make sure that reading from the register returns the correct value.
        TEST_ASSERT(lapic->readRegister(reg) == randValue);

        // Test writing the register and re-reading it again.
        if (LocalApic::isRegisterWritable(reg)) {
            // Write the register with the 1-complement.
            lapic->writeRegister(reg, ~randValue);
            // Re-read the register, we should get the new value.
            TEST_ASSERT(lapic->readRegister(reg) == ~randValue);
        }
    }
    return SelfTests::TestResult::Success;
}

// Test the setupTimer() function.
SelfTests::TestResult localApicSetupTimerTest() {
    MockLapicGuard guard;
    LocalApic* const lapic(guard.lapic);
    Vector const vector(0xcc);
    VirAddr const baseVAddr(Paging::toVirAddr(lapic->m_base));

    // Set the TIC register to some non-zero value to check that setupTimer
    // stops the current timer.
    u32 * const ticPtr((baseVAddr
        + static_cast<u64>(LocalApic::Register::TimerInitialCount)).ptr<u32>());
    *ticPtr = 0xdeadbeef;

    // Set the Timer LVT register to ~0ULL so we can check that the setupTimer()
    // is not overwriting reserved bits.
    u32 * const regPtr((baseVAddr
        + static_cast<u64>(LocalApic::Register::TimerLocalVectorTableEntry))
        .ptr<u32>());
    *regPtr = u32(~0ULL);
    u32 const reservedMask(0xfffcff00);

    // Case 1: One shot timer.
    lapic->setupTimer(LocalApic::TimerMode::OneShot, vector);
    u32 const expTimerLvtValue1(vector.raw());
    TEST_ASSERT(
        (lapic->readRegister(LocalApic::Register::TimerLocalVectorTableEntry)
         & ~reservedMask)
        == expTimerLvtValue1);
    // Check that no reserved bit was overwritten.
    TEST_ASSERT((*regPtr & reservedMask) == reservedMask);
    // Check that the timer was stopped.
    TEST_ASSERT(!*ticPtr);
    *ticPtr = 0xdeadbeef;

    // Case 2: Periodic timer.
    lapic->setupTimer(LocalApic::TimerMode::Periodic, vector);
    u32 const expTimerLvtValue2((1 << 17) | vector.raw());
    TEST_ASSERT(
        (lapic->readRegister(LocalApic::Register::TimerLocalVectorTableEntry)
         & ~reservedMask)
        == expTimerLvtValue2);
    // Check that no reserved bit was overwritten.
    TEST_ASSERT((*regPtr & reservedMask) == reservedMask);
    // Check that the timer was stopped.
    TEST_ASSERT(!*ticPtr);

    return SelfTests::TestResult::Success;
}

// Check the resetTimer() function.
SelfTests::TestResult localApicResetTimerTest() {
    MockLapicGuard guard;
    LocalApic* const lapic(guard.lapic);

    // Mock setting-up a timer.
    lapic->setupTimer(LocalApic::TimerMode::Periodic, Vector(123));

    // Check the TimerClockDivisor enum.
    using Div = LocalApic::TimerClockDivisor;
    TEST_ASSERT(static_cast<u8>(Div::DivideBy2)     == 0b000);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy4)     == 0b001);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy8)     == 0b010);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy16)    == 0b011);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy32)    == 0b100);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy64)    == 0b101);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy128)   == 0b110);
    TEST_ASSERT(static_cast<u8>(Div::DivideBy1)     == 0b111);

    Div const divisors[] = {
        Div::DivideBy2,
        Div::DivideBy4,
        Div::DivideBy8,
        Div::DivideBy16,
        Div::DivideBy32,
        Div::DivideBy64,
        Div::DivideBy128,
        Div::DivideBy1,
    };
    u32 const ticks(0xdeadbeef);
    VirAddr const baseVAddr(Paging::toVirAddr(lapic->m_base));
    u32 * const regPtr((baseVAddr
        + static_cast<u64>(LocalApic::Register::TimerDivideConfiguration))
          .ptr<u32>());
    // Test all divisors.
    for (Div const div : divisors) {
        // Set the reserved bits in the divide configuration register.
        *regPtr = u32(~0ULL);

        // Reset the timer.
        lapic->resetTimer(ticks, div);

        // Check that the initial count has been set.
        TEST_ASSERT(lapic->readRegister(LocalApic::Register::TimerInitialCount)
                    == ticks);
        // Check that the divisor has been set and no reserved bit has been
        // overwritten.
        u32 const rawDiv(static_cast<u8>(div));
        u32 const expValue(~11 | ((rawDiv & 0b100) << 3) | (rawDiv & 3));
        TEST_ASSERT(
            lapic->readRegister(LocalApic::Register::TimerDivideConfiguration)
            == expValue);
    }
    return SelfTests::TestResult::Success;
}

// Run the Local APIC tests.
void LocalApic::Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, localApicRegisterReadWriteTest);
    RUN_TEST(runner, localApicSetupTimerTest);
    RUN_TEST(runner, localApicResetTimerTest);
}
}
