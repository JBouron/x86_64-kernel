// Paging related tests.
#include <paging/paging.hpp>
#include <framealloc/framealloc.hpp>
#include <interrupts/interrupts.hpp>
#include <selftests/macros.hpp>
#include <paging/addrspace.hpp>

namespace Paging {

// Test for the Paging::map() function.
SelfTests::TestResult mapTest() {
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

    Paging::PageAttr const attrs(Paging::PageAttr::Writable);
    Paging::map(startVAddr, startPAddr, attrs, numPages);

    u64 const * readIdPtr(startPAddr.toVir().ptr<u64>());
    u64 const * readMapPtr(startVAddr.ptr<u64>());
    for (u64 i(0); i < mapSize / sizeof(u64); ++i) {
        TEST_ASSERT(*(readIdPtr++)==*(readMapPtr++));
    }
    Paging::unmap(startVAddr, numPages);
    return SelfTests::TestResult::Success;
}

// Test setting attributes when mapping. More specifically test that a page
// mapped without the PageAttr::Writable attribute triggers a page-fault when
// written-to. This is the only attribute we can test for now as we don't have a
// userspace yet.
SelfTests::TestResult mapAttrsTest() {
    // The test executes the following:
    //  1. Alloc a physical page frame.
    //  2. Map the frame to some virtual address in readonly.
    //  3. Register a temporary page fault handler.
    //  4. Write into the readonly virtual address mapped in #2.
    //  5. Remap the virtual address from #2 as read/write from within the
    //  pagefault handler.
    //
    // The following variable must be static as they need to be accessible from
    // the temporary page-fault handler.
    // The physical address of the allocated frame.
    static PhyAddr paddr;
    // The address where the frame is mapped to in virtual memory. This is also
    // the expected faulting address.
    static VirAddr const vaddr(0xbadbeef000);
    // Set by the handler to indicate that it got called.
    static bool gotPageFault;
    // The value of CR2 as seen by the handler at the time of the fault.
    static VirAddr pageFaultCr2;

    // Alloc the physical frame.
    Res<FrameAlloc::Frame> const allocRes(FrameAlloc::alloc());
    TEST_ASSERT(!!allocRes);
    paddr = allocRes->addr();

    // Map the frame as read-only.
    PageAttr const attrs(PageAttr::None);
    TEST_ASSERT(!Paging::map(vaddr, paddr, attrs, 1));

    gotPageFault = false;
    pageFaultCr2 = 0x0;

    // Setup a handler for page-faults.
    auto const pageFaultHandler([](Interrupts::Vector const vector,
                                   Interrupts::Frame const& frame) {
        ASSERT(vector == 14);
        pageFaultCr2 = Cpu::cr2();
        gotPageFault = true;

        // The error code should show a present page and a write violation,
        // hence 0x3.
        ASSERT(frame.errorCode == 0x3);

        // Change the mapping to be writable.
        PageAttr const attrs(PageAttr::Writable);
        ASSERT(!Paging::map(vaddr, paddr, attrs, 1));
        Log::debug("Set page as writable");
    });
    TemporaryInterruptHandlerGuard guard(Interrupts::Vector(14),
                                         pageFaultHandler);

    // Trigger the page-fault.
    u8* const ptr(vaddr.ptr<u8>());
    *ptr = 0;

    TEST_ASSERT(gotPageFault);
    TEST_ASSERT(pageFaultCr2 == vaddr);

    // Unmap the test page.
    Paging::unmap(vaddr, 1);

    // Free the frame.
    FrameAlloc::free(*allocRes);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult unmapTest() {
    // This test performs the following:
    //  1. Map a virtual page as writable.
    //  2. Write into the page, no page-fault expected.
    //  3. Un-map the page.
    //  4. Write into the page again.
    //  5. Assert that the write above triggered a page-fault. The page-fault
    //  handler re-maps the page so that the offending instruction can complete
    //  and finish the test.
    // The following variable must be static as they need to be accessible from
    // the temporary page-fault handler.
    // The physical address of the allocated frame.
    static PhyAddr paddr;
    // The address where the frame is mapped to in virtual memory. This is also
    // the expected faulting address.
    static VirAddr const vaddr(0xbadbeef000);
    // Set by the handler to indicate that it got called.
    static bool gotPageFault;
    // The value of CR2 as seen by the handler at the time of the fault.
    static VirAddr pageFaultCr2;
    // The value of the error code as seen by the handler at the time of the
    // fault.
    static u64 errorCode;

    // Alloc the physical frame.
    Res<FrameAlloc::Frame> const allocRes(FrameAlloc::alloc());
    TEST_ASSERT(!!allocRes);
    paddr = allocRes->addr();

    // Map the frame as read-write.
    PageAttr const attrs(PageAttr::Writable);
    TEST_ASSERT(!Paging::map(vaddr, paddr, attrs, 1));

    gotPageFault = false;
    pageFaultCr2 = 0x0;
    errorCode = 0;

    // Setup a handler for page-faults.
    auto const pageFaultHandler([](Interrupts::Vector const vector,
                                   Interrupts::Frame const& frame) {
        ASSERT(vector == 14);
        pageFaultCr2 = Cpu::cr2();
        gotPageFault = true;
        errorCode = frame.errorCode;

        // Re-map the page.
        PageAttr const attrs(PageAttr::Writable);
        ASSERT(!Paging::map(vaddr, paddr, attrs, 1));
        Log::debug("Set page as writable");
    });
    TemporaryInterruptHandlerGuard guard(Interrupts::Vector(14),
                                         pageFaultHandler);

    // Write to the page, no page fault expected since it is mapped and
    // writable.
    u8* const ptr(vaddr.ptr<u8>());
    *ptr = 0xff;

    TEST_ASSERT(!gotPageFault);

    // Now unmap the page.
    Paging::unmap(vaddr, 1);

    // Write to the page again, this should trigger a page-fault. The page fault
    // handler will remap this page so that we can complete.
    *ptr = 0xaa;
    TEST_ASSERT(gotPageFault);
    TEST_ASSERT(pageFaultCr2 == vaddr);
    TEST_ASSERT(errorCode == 0x2);

    // Unmap the test page.
    Paging::unmap(vaddr, 1);

    // Free the frame.
    FrameAlloc::free(*allocRes);
    return SelfTests::TestResult::Success;
}

// Check that switch between address spaces work as expected.
SelfTests::TestResult addrSpaceTest() {
    // This test performs the following steps:
    //  1. Allocate AddrSpace A.
    //  2. Switch to A1 and map address X
    //  3. Switch back to the previous address space.
    //  4. Attempt to write to X, this should generate a page fault.
    //  5. In the page-fault handler, switch to address space A and return.
    //  6. The write should now complete.
    static Ptr<AddrSpace> addrSpace;
    addrSpace = AddrSpace::New().value();

    u64 const oldCr3(Cpu::cr3());
    u64 const pml4Mask(~(PAGE_SIZE - 1));
    PhyAddr const oldPml4(oldCr3 & pml4Mask);

    // Sanity check, the address space uses a different PML4.
    TEST_ASSERT(oldPml4 != addrSpace->pml4Address());

    // Switch to the new address space.
    AddrSpace::switchAddrSpace(addrSpace);
    TEST_ASSERT((Cpu::cr3() & pml4Mask) == addrSpace->pml4Address().raw());

    // Map the address.
    Frame const tempFrame(FrameAlloc::alloc().value());
    // Make sure this is a user address so we don't interfere with the original
    // address space.
    static VirAddr const tempAddr(0xcafe000);

    TEST_ASSERT(!Paging::map(tempAddr,
                             tempFrame.addr(),
                             Paging::PageAttr::Writable,
                             1));

    // At this point we should be able to write.
    *tempAddr.ptr<u64>() = 0xdead;

    // Manually switch back to the previous address space. We cannot use
    // AddrSpace::switchAddrSpace since we don't have an AddrSpace instance for
    // the boot address space.
    AddrSpace::switchAddrSpace(oldPml4);

    static u64 pageFaultAddr;

    auto const pageFaultHandler([](Interrupts::Vector const,
                                   Interrupts::Frame const&) {
        pageFaultAddr = Cpu::cr2();
        Log::debug("Page fault on address {x}, cr3 = {x}", pageFaultAddr,
                   Cpu::cr3());
        AddrSpace::switchAddrSpace(addrSpace);
    });
    TemporaryInterruptHandlerGuard gd(Interrupts::Vector(14), pageFaultHandler);

    // Try to write at the tempAddr. Since we reverted back to the original
    // address space this should generate a page-fault.
    TEST_ASSERT((Cpu::cr3() & pml4Mask) != addrSpace->pml4Address().raw());
    pageFaultAddr = 0;
    *tempAddr.ptr<u64>() = 0xbeef;
    // At this point the page fault handler was called and changed our addrses
    // space so that the write could complete.
    TEST_ASSERT((Cpu::cr3() & pml4Mask) == addrSpace->pml4Address().raw());
    TEST_ASSERT(pageFaultAddr == tempAddr.raw());
    TEST_ASSERT(*tempAddr.ptr<u64>() == 0xbeef);

    // Revert to the original address space before returning.
    AddrSpace::switchAddrSpace(oldPml4);

    // Make sure the AddrSpace is freed. This will automatically un-map the
    // mapping above and de-allocate all page-tables that were allocated for
    // this mapping.
    addrSpace = Ptr<AddrSpace>();

    return SelfTests::TestResult::Success;
}

// Run paging tests.
void Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, mapTest);
    RUN_TEST(runner, mapAttrsTest);
    RUN_TEST(runner, unmapTest);
    RUN_TEST(runner, addrSpaceTest);
}

}
