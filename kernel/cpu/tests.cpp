// Tests for the Cpu namespace.
#include <cpu/cpu.hpp>

namespace Cpu {

// Test the Cpu::lgdt and Cpu::sgdt macros. We expect the sgdt() to return the
// last TableDesc loaded with lgdt().
SelfTests::TestResult lgdtSgdtTest() {
    // A dummy GDT to be used for this test. The actual content of the gdt do
    // not matter, as long as all entries are marked as present.
    static u64 dummyGdt[] = { 0x0, (1ULL << 47), (1ULL << 47), (1ULL << 47) };

    Cpu::TableDesc const origGdt(Cpu::sgdt());
    
    u64 const base(reinterpret_cast<u64>(dummyGdt));
    u64 const limit(sizeof(dummyGdt) - 1);
    Cpu::TableDesc const dummyDesc(base, limit);

    // Load the dummy GDT.
    Cpu::lgdt(dummyDesc);

    // Read the current GDTR and compare against the expected GDTR (e.g.
    // dummyDesc).
    TEST_ASSERT(Cpu::sgdt() == dummyDesc);

    // Restore the original GDT.
    Cpu::lgdt(origGdt);
    return SelfTests::TestResult::Success;
}

// Test the Cpu::readSegmentReg() and Cpu::writeSegmentReg() functions. We
// expect the read function to return the last written value.
SelfTests::TestResult readWriteSegmentRegTest() {
    // Get a copy of the current GDT as well as all the segment registers.
    Cpu::TableDesc const origGdt(Cpu::sgdt());
    Cpu::SegmentSel const origCs(Cpu::readSegmentReg(Cpu::SegmentReg::Cs));
    Cpu::SegmentSel const origDs(Cpu::readSegmentReg(Cpu::SegmentReg::Ds));
    Cpu::SegmentSel const origEs(Cpu::readSegmentReg(Cpu::SegmentReg::Es));
    Cpu::SegmentSel const origFs(Cpu::readSegmentReg(Cpu::SegmentReg::Fs));
    Cpu::SegmentSel const origGs(Cpu::readSegmentReg(Cpu::SegmentReg::Gs));
    Cpu::SegmentSel const origSs(Cpu::readSegmentReg(Cpu::SegmentReg::Ss));

    // A dummy GDT so that we can use deterministic selectors.
    static u64 dummyGdt[] = {
        // NULL desc.
        0x0,
        // Data segment #1.
        (1ULL << 53) | (1ULL << 47) | (1ULL << 44) | (1ULL << 41),
        // Data segment #2.
        (1ULL << 53) | (1ULL << 47) | (1ULL << 44) | (1ULL << 41),
        // Code segment #1.
        (1ULL << 53) | (1ULL << 47) | (1ULL << 44) | (1ULL << 43),
    };

    u64 const base(reinterpret_cast<u64>(dummyGdt));
    u64 const limit(sizeof(dummyGdt) - 1);
    Cpu::TableDesc const dummyDesc(base, limit);
    Cpu::lgdt(dummyDesc);

    // Code segment.
    Cpu::SegmentSel const newCs(3, false, Cpu::PrivLevel::Ring0);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Cs, newCs);
    TEST_ASSERT(Cpu::readSegmentReg(Cpu::SegmentReg::Cs) == newCs);

    // Change a data segment twice, setting it to the first and second entry of
    // the dummy GDT. Check that readSegmentReg reports the correct value.
    auto const testDataSeg([](Cpu::SegmentReg const reg) {
        // Set segment to entry at index 1.
        Cpu::SegmentSel const newSel(1, false, Cpu::PrivLevel::Ring0);
        Cpu::writeSegmentReg(reg, newSel);
        TEST_ASSERT(Cpu::readSegmentReg(reg) == newSel);

        // Set segment to entry at index 2.
        Cpu::SegmentSel const newSel2(2, false, Cpu::PrivLevel::Ring0);
        Cpu::writeSegmentReg(reg, newSel2);
        TEST_ASSERT(Cpu::readSegmentReg(reg) == newSel2);

        return SelfTests::TestResult::Success;
    });

    TEST_ASSERT(testDataSeg(SegmentReg::Ds) == SelfTests::TestResult::Success);
    TEST_ASSERT(testDataSeg(SegmentReg::Es) == SelfTests::TestResult::Success);
    TEST_ASSERT(testDataSeg(SegmentReg::Fs) == SelfTests::TestResult::Success);
    TEST_ASSERT(testDataSeg(SegmentReg::Gs) == SelfTests::TestResult::Success);
    TEST_ASSERT(testDataSeg(SegmentReg::Ss) == SelfTests::TestResult::Success);

    Cpu::lgdt(origGdt);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Cs, origCs);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Ds, origDs);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Es, origEs);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Fs, origFs);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Gs, origGs);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Ss, origSs);
    return SelfTests::TestResult::Success;
}

// Test for the lidt() and sidt() functions.
SelfTests::TestResult lidtSidtTest() {
    Cpu::TableDesc const origIdt(Cpu::sidt());

    static u64 dummyIdt[] = { 0x0, 0x0, 0x0 };
    u64 const base(reinterpret_cast<u64>(dummyIdt));
    u16 const limit(sizeof(dummyIdt) - 1);
    Cpu::TableDesc const dummyIdtDesc(base, limit);
    Cpu::lidt(dummyIdtDesc);
    TEST_ASSERT(Cpu::sidt() == dummyIdtDesc);

    // Restore the original IDTR.
    Cpu::lidt(origIdt);
    return SelfTests::TestResult::Success;
}

// Run the tests under this namespace.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, lgdtSgdtTest);
    RUN_TEST(runner, readWriteSegmentRegTest);
    RUN_TEST(runner, lidtSidtTest);
}
}
