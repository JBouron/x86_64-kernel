// CPU functions related tests.

#include <selftests/selftests.hpp>
#include <cpu/cpu.hpp>

namespace SelfTests {

// Test the Cpu::lgdt and Cpu::sgdt macros. We expect the sgdt() to return the
// last TableDesc loaded with lgdt().
TestResult lgdtSgdtTest() {
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
    return TEST_SUCCESS;
}

}
