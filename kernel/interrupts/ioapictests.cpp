// Tests related to I/O APIC support.
#include <framealloc/framealloc.hpp>
#include <paging/paging.hpp>
#include <selftests/macros.hpp>

#include "ioapic.hpp"

namespace Interrupts {

// Tests that RedirectionTableEntry correctly computes the 64-bit values to be
// loaded in the redirection table.
SelfTests::TestResult ioApicRedirectionTableEntryTest() {
    // The default constructor should create a value that is the default
    // redirection table entry value from the HW, e.g. all bits to 0 except the
    // mask bit.
    IoApic::RedirectionTableEntry const defaultValue;
    TEST_ASSERT(defaultValue.raw() == (1 << 16));

    // Check the values of the enums. Useful to catch the situation where an
    // enum was changed by accident.
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::Fixed) == 0b000);
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::LowestPriority) == 0b001);
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::Smi) == 0b010);
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::Nmi) == 0b100);
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::Init) == 0b101);
    TEST_ASSERT(static_cast<u8>(IoApic::DeliveryMode::ExtInt) == 0b111);
    TEST_ASSERT(static_cast<u8>(IoApic::DestinationMode::Logical) == 1);
    TEST_ASSERT(static_cast<u8>(IoApic::DestinationMode::Physical) == 0);
    TEST_ASSERT(static_cast<u8>(IoApic::InputPinPolarity::ActiveHigh) == 0);
    TEST_ASSERT(static_cast<u8>(IoApic::InputPinPolarity::ActiveLow) == 1);
    TEST_ASSERT(static_cast<u8>(IoApic::TriggerMode::Edge) == 0);
    TEST_ASSERT(static_cast<u8>(IoApic::TriggerMode::Level) == 1);

    // Now make sure that RedirectionTableEntry is constructing the right 64-bit
    // value given the configuration. Construct a RedirectionTableEntry with
    // random configuration to route vector `vector` to LAPIC ID `dest`.
    // FIXME: Some of the configuration created here are not valid, for instance
    // using SMI delivery mode should be edge triggered with vector 0.
    Vector const vector(13);
    IoApic::Dest const dest(7);

    IoApic::DeliveryMode const deliveryModes[] = {
        IoApic::DeliveryMode::Fixed,
        IoApic::DeliveryMode::LowestPriority,
        IoApic::DeliveryMode::Smi,
        IoApic::DeliveryMode::Nmi,
        IoApic::DeliveryMode::Init,
        IoApic::DeliveryMode::ExtInt,
    };
    IoApic::DestinationMode const destinationModes[] = {
        IoApic::DestinationMode::Logical,
        IoApic::DestinationMode::Physical,
    };
    IoApic::InputPinPolarity const polarities[] = {
        IoApic::InputPinPolarity::ActiveHigh,
        IoApic::InputPinPolarity::ActiveLow,
    };
    IoApic::TriggerMode const triggerModes[] = {
        IoApic::TriggerMode::Edge,
        IoApic::TriggerMode::Level,
    };

    // Behold the 4 nested loops.
    for (IoApic::DeliveryMode const& delMode : deliveryModes) {
        for (IoApic::DestinationMode const& destMode : destinationModes) {
            for (IoApic::InputPinPolarity const& pol : polarities) {
                for (IoApic::TriggerMode const& trig : triggerModes) {
                    IoApic::RedirectionTableEntry entry(vector, delMode,
                        destMode, pol, trig, dest);
                    u64 const expectedRaw(u64(dest) << 56
                       | (u64(static_cast<u8>(trig)) << 15)
                       | (u64(static_cast<u8>(pol)) << 13)
                       | (u64(static_cast<u8>(destMode)) << 11)
                       | (u64(static_cast<u8>(delMode)) << 8)
                       | vector.raw());
                    TEST_ASSERT(entry.raw() == expectedRaw);

                    // Test toggling the mask bit.
                    entry.setMasked(true);
                    TEST_ASSERT(entry.raw() == (expectedRaw | (1 << 16)));
                    entry.setMasked(false);
                    TEST_ASSERT(entry.raw() == expectedRaw);

                    // Test creating an entry from the raw value.
                    IoApic::RedirectionTableEntry const fromRaw(expectedRaw);
                    TEST_ASSERT(fromRaw.raw() == expectedRaw);
                    IoApic::RedirectionTableEntry const fromRawMasked(
                        expectedRaw | (1 << 16));
                    TEST_ASSERT(fromRawMasked.raw()
                                == (expectedRaw | (1 << 16)));
                }
            }
        }
    }
    return SelfTests::TestResult::Success;
}

class MockIoApic : public IoApic {
public:
    MockIoApic() : IoApic(FrameAlloc::alloc().value().addr()) {
        for (u64 i(0); i < sizeof(registers) / sizeof(*registers); ++i) {
            registers[i] = 0;
        }
        // Set the registers to their default values, per the spec.
        registers[static_cast<u8>(IoApic::Register::IOAPICID)] = 0x0;
        registers[static_cast<u8>(IoApic::Register::IOAPICVER)] = 0x00170011;
        registers[static_cast<u8>(IoApic::Register::IOAPICARB)] = 0x0;
    }

    ~MockIoApic() {
        // FIXME: Fixup the page attribute changes in the Direct Map caused by
        // the IoApic constructor.
        FrameAlloc::free(FrameAlloc::Frame(m_base.raw()));
    }

    // The current value of the registers, indexed by Register. Calling
    // readRegister() and writeRegister() reads/writes into this table.
    u32 registers[0x40];

    // Record the last two registers that have been written. lastTwoWrites[0]
    // indicates the last register written-to, lastTwoWrites[1] is the one
    // before that.
    Register lastTwoWrites[2];

    // Read an I/O APIC register.
    // @param src: The register to read.
    // @return: The current value of the register `src`.
    virtual u32 readRegister(Register const src) const {
        return registers[static_cast<u8>(src)];
    }

    // Write into an I/O APIC register. Note: This function does NOT skip
    // reserved bits in registers, it is the responsibility of the caller to
    // make sure not to change the value of a reserved bit.
    // @param dest: The register to write into.
    // @param value: The value to write into the destination register.
    virtual void writeRegister(Register const dest, u32 const value) {
        lastTwoWrites[1] = lastTwoWrites[0];
        lastTwoWrites[0] = dest;
        registers[static_cast<u8>(dest)] = value;
    }
};

// Test that id(), version() and arbitrationId() are correctly parsing their
// associated registers.
SelfTests::TestResult ioApicReadRegisterTest() {
    // Check that the Register enum is correctly defined.
    TEST_ASSERT(static_cast<u8>(IoApic::Register::IOAPICID) == 0x00);
    TEST_ASSERT(static_cast<u8>(IoApic::Register::IOAPICVER) == 0x01);
    TEST_ASSERT(static_cast<u8>(IoApic::Register::IOAPICARB) == 0x02);
    TEST_ASSERT(static_cast<u8>(IoApic::Register::IOREDTBL_BASE) == 0x10);

    MockIoApic ioApic;
    // Check ID is correctly parsed from register.
    ioApic.writeRegister(IoApic::Register::IOAPICID, 0xfcffffff);
    TEST_ASSERT(ioApic.id() == 0xc);

    // Check Version and numInterruptSources are correctly parsed from register.
    ioApic.writeRegister(IoApic::Register::IOAPICVER, 0xff13ff69);
    TEST_ASSERT(ioApic.version() == 0x69);
    TEST_ASSERT(ioApic.numInterruptSources() == 0x13);

    // Check the ARB ID is corretly parsed.
    ioApic.writeRegister(IoApic::Register::IOAPICARB, 0xfaffffff);
    TEST_ASSERT(ioApic.arbitrationId() == 0xa);
    return SelfTests::TestResult::Success;
}

// Check that the implementation correctly reads and writes entries in the
// redirection table.
SelfTests::TestResult ioApicReadWriteRedirectionTableTest() {
    MockIoApic ioApic;

    // Write a random redirection table entry.
    IoApic::RedirectionTableEntry const entry(
        Vector(17),
        IoApic::DeliveryMode::ExtInt,
        IoApic::DestinationMode::Logical,
        IoApic::InputPinPolarity::ActiveLow,
        IoApic::TriggerMode::Edge,
        0xf);

    u8 const inputPin(7);
    ioApic.writeRedirectionTable(inputPin, entry);
    
    // First check that the low dword was written-to first.
    IoApic::Register const lowDwordReg(static_cast<IoApic::Register>(
        static_cast<u8>(IoApic::Register::IOREDTBL_BASE) + inputPin * 2));
    IoApic::Register const highDwordReg(static_cast<IoApic::Register>(
        static_cast<u8>(IoApic::Register::IOREDTBL_BASE) + inputPin * 2 + 1));

    // Last register was the high DWORD.
    TEST_ASSERT(ioApic.lastTwoWrites[0] == highDwordReg);
    // Second to last register was the low DWORD.
    TEST_ASSERT(ioApic.lastTwoWrites[1] == lowDwordReg);

    // Check that the correct value has been written to the correct registers.
    u64 const entryRaw(entry.raw());
    TEST_ASSERT(ioApic.registers[static_cast<u8>(lowDwordReg)]
                == (entryRaw & 0xffffffff));
    TEST_ASSERT(ioApic.registers[static_cast<u8>(highDwordReg)]
                == (entryRaw >> 32));
    
    // Check that reading from the table returns the same value.
    TEST_ASSERT(ioApic.readRedirectionTable(inputPin).raw() == entry.raw());
    return SelfTests::TestResult::Success;
}

// Check that writing an entry in the redirection table does not clobber the
// reserved bits.
SelfTests::TestResult ioApicWriteRedirectionTableEntryReservedBitTest() {
    MockIoApic ioApic;
    // The test input pin is zero so that the two registers used by the
    // redirection entry are IOREDTBL_BASE and IOREDTBL_BASE+1.
    u8 const inputPin(0);
    u8 const lowDwordReg(static_cast<u8>(IoApic::Register::IOREDTBL_BASE));
    u8 const highDwordReg(lowDwordReg + 1);

    // Set the all the entry's bits to 1 (including the reserved bits).
    ioApic.registers[lowDwordReg] = u32(~0ULL);
    ioApic.registers[highDwordReg] = u32(~0ULL);

    // Write a random redirection table entry.
    IoApic::RedirectionTableEntry const entry(
        Vector(17),
        IoApic::DeliveryMode::ExtInt,
        IoApic::DestinationMode::Logical,
        IoApic::InputPinPolarity::ActiveLow,
        IoApic::TriggerMode::Edge,
        0xf);

    ioApic.writeRedirectionTable(inputPin, entry);

    // Check that the reserved bits are still 1's.
    TEST_ASSERT((ioApic.registers[lowDwordReg] & 0xfffe0000) == 0xfffe0000);
    TEST_ASSERT((ioApic.registers[highDwordReg] & 0x00ffffff) == 0x00ffffff);

    return SelfTests::TestResult::Success;
}

// Check that calling setInterruptSourceMask() only changes the value of the
// mask bit in the associated redirection table entry.
SelfTests::TestResult ioApicMaskInterruptSourceTest() {
    MockIoApic ioApic;

    // Write a random redirection table entry.
    IoApic::RedirectionTableEntry const entry(
        Vector(17),
        IoApic::DeliveryMode::ExtInt,
        IoApic::DestinationMode::Logical,
        IoApic::InputPinPolarity::ActiveLow,
        IoApic::TriggerMode::Edge,
        0xf);

    u8 const inputPin(7);
    ioApic.writeRedirectionTable(inputPin, entry);

    // Mask the interrupt source.
    ioApic.setInterruptSourceMask(IoApic::InputPin(inputPin), true);
    TEST_ASSERT(ioApic.readRedirectionTable(inputPin).raw()
                == (entry.raw() | (1 << 16)));

    // Unmask the interrupt source.
    ioApic.setInterruptSourceMask(IoApic::InputPin(inputPin), false);
    TEST_ASSERT(ioApic.readRedirectionTable(inputPin).raw() == entry.raw());

    return SelfTests::TestResult::Success;
}

// Check that calling redirectInterrupt correctly configures the redirection
// table entry associated with the input pin.
SelfTests::TestResult ioApicRedirectInterruptTest() {
    MockIoApic ioApic;

    IoApic::InputPin const inputPin(5);
    ioApic.redirectInterrupt(inputPin,
                             IoApic::OutVector(17),
                             IoApic::DeliveryMode::ExtInt,
                             IoApic::DestinationMode::Logical,
                             IoApic::InputPinPolarity::ActiveLow,
                             IoApic::TriggerMode::Edge,
                             0x2);

    IoApic::RedirectionTableEntry const expectedEntry(
        Vector(17),
        IoApic::DeliveryMode::ExtInt,
        IoApic::DestinationMode::Logical,
        IoApic::InputPinPolarity::ActiveLow,
        IoApic::TriggerMode::Edge,
        0x2);

    TEST_ASSERT(ioApic.readRedirectionTable(inputPin.raw()).raw()
                == expectedEntry.raw());
    return SelfTests::TestResult::Success;
}

void IoApic::Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, ioApicRedirectionTableEntryTest);
    RUN_TEST(runner, ioApicReadRegisterTest);
    RUN_TEST(runner, ioApicReadWriteRedirectionTableTest);
    RUN_TEST(runner, ioApicWriteRedirectionTableEntryReservedBitTest);
    RUN_TEST(runner, ioApicMaskInterruptSourceTest);
    RUN_TEST(runner, ioApicRedirectInterruptTest);
}
}
