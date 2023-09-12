// Tests for segmentation.
#include <memory/segmentation.hpp>

namespace Memory::Segmentation {

// Check that Descriptor values are correctly computed.
SelfTests::TestResult segmentationDescriptorTest() {
    TEST_ASSERT(static_cast<u64>(Descriptor::Type::DataReadOnly) == 0b0000);
    TEST_ASSERT(static_cast<u64>(Descriptor::Type::DataReadWrite) == 0b0010);
    // FIXME: Add support for expand-down segments. This might require some
    // more work to correctly set the D/B bit.
    //DataExpandDownReadOnly = 0x4,
    //DataExpandDownReadWrite = 0x6,
    TEST_ASSERT(static_cast<u64>(Descriptor::Type::CodeExecuteOnly) == 0b1000);
    TEST_ASSERT(static_cast<u64>(Descriptor::Type::CodeExecuteReadable)
                == 0b1010);
    TEST_ASSERT(static_cast<u64>(Descriptor::Type::CodeConformingExecuteOnly)
                == 0b1100);
    TEST_ASSERT(
        static_cast<u64>(Descriptor::Type::CodeConformingExecuteReadable)
        == 0b1110);

    TEST_ASSERT(static_cast<u64>(Descriptor::Granularity::Byte) == 0);
    TEST_ASSERT(static_cast<u64>(Descriptor::Granularity::Page) == 1);

    Cpu::PrivLevel const dpls[] = {
        Cpu::PrivLevel::Ring0,
        Cpu::PrivLevel::Ring1,
        Cpu::PrivLevel::Ring2,
        Cpu::PrivLevel::Ring3,
    };

    Descriptor::Type const types[] = {
        Descriptor::Type::DataReadOnly,
        Descriptor::Type::DataReadWrite,
        // FIXME: Add support for expand-down segments. This might require some
        // more work to correctly set the D/B bit.
        //DataExpandDownReadOnly = 0x4,
        //DataExpandDownReadWrite = 0x6,
        Descriptor::Type::CodeExecuteOnly,
        Descriptor::Type::CodeExecuteReadable,
        Descriptor::Type::CodeConformingExecuteOnly,
        Descriptor::Type::CodeConformingExecuteReadable,
    };

    Descriptor::Granularity const grans[] = {
        Descriptor::Granularity::Byte,
        Descriptor::Granularity::Page,
    };

    Descriptor::Base const base(0xdeadbeef);
    Descriptor::Limit const limit(0xcaffe);

    // Descriptor32 test.
    for (auto const dpl : dpls) {
        for (auto const type : types) {
            for (auto const g : grans) {
                Descriptor const desc(Descriptor32(base, limit, dpl, type, g));
                u64 const raw(desc.raw());
                u64 const exp(
                    (u64(base >> 24) << 56)
                    | (static_cast<u64>(g) << 55)
                    | (u64(1) << 54)
                    | ((limit.raw() >> 16) << 48)
                    | (u64(1) << 47)
                    | (static_cast<u64>(dpl) << 45)
                    | (u64(1) << 44)
                    | (static_cast<u64>(type) << 40)
                    | ((u64(base >> 16) & 0xff) << 32)
                    | ((u64(base & 0xffff) << 16))
                    | (limit.raw() & 0xffff));
                TEST_ASSERT(raw == exp);
            }
        }
    }

    // Descriptor64 test.
    for (auto const dpl : dpls) {
        for (auto const type : types) {
            Descriptor const desc(Descriptor64(dpl, type));
            u64 const raw(desc.raw());
            u64 const exp(
                (u64(1) << 53)
                | (u64(1) << 47)
                | (static_cast<u64>(dpl) << 45)
                | (u64(1) << 44)
                | (static_cast<u64>(type) << 40));
            TEST_ASSERT(raw == exp);
        }
    }
    return SelfTests::TestResult::Success;
}

// Run segmentation tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, segmentationDescriptorTest);
}
}
