// Tests for paging functions.

#include <selftests/selftests.hpp>
#include <paging/paging.hpp>

namespace SelfTests {

// Test for the Paging::map() function.
TestResult mapTest() {
    // For this test we map the bootloader's memory to some virtual address,
    // then we read both from the mapped address and the direct map and compare
    // what we read.
    VirAddr const startVAddr(0xcafecafe000);
    // Take the next page boundary in the bootloader's memory, map until we
    // reach the EBDA.
    PhyAddr const startPAddr(0x8000);
    u64 const mapSize(0x80000 - startPAddr.raw());
    TEST_ASSERT(!(mapSize % PAGE_SIZE));
    u64 const numPages(mapSize / PAGE_SIZE);

    Paging::map(startVAddr, startPAddr, numPages);

    u64 const * readIdPtr(Paging::toVirAddr(startPAddr).ptr<u64>());
    u64 const * readMapPtr(startVAddr.ptr<u64>());
    for (u64 i(0); i < mapSize / sizeof(u64); ++i) {
        TEST_ASSERT(*(readIdPtr++)==*(readMapPtr++));
    }
    // FIXME: Eventually we should unmap the pages here.
    return TEST_SUCCESS;
}

}
