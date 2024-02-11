// Everything related to paging.

#pragma once
#include <util/util.hpp>
#include <util/err.hpp>
#include <bootstruct.hpp>
#include <selftests/selftests.hpp>
#include <util/addr.hpp>

namespace Paging {

// The direct map
// ==============
// The direct map maps the entire physical memory as R/W starting at address
// DIRECT_MAP_START_VADDR, hence to access physical address X we use virtual
// address X + DIRECT_MAP_START_VADDR.
static constexpr u64 DIRECT_MAP_START_VADDR = 0xffff800000000000;

// Initialize paging.
// This function creates the direct map.
void Init(BootStruct const& bootStruct);

// Configure paging for the current cpu (set control regs, ...).
// Automatically called by Init() for the BSP.
void InitCurrCpu();

// Run paging tests.
void Test(SelfTests::TestRunner& runner);

// Attributes of pages when mapping virtual addresses to physical addresses.
// Can use a combination of attributes using the operator|.
enum class PageAttr : u64 {
    // Helper function to set the value of a PageAttr to 0, e.g. no bits.
    None = 0,
    // The page is writable.
    Writable = 2,
    // The page can be accessed from ring 3.
    User = 4,
    // Memory type of the page.
    WriteThrough = 8,
    CacheDisable = 16,
    // Indicate if this page is global.
    Global = 32,
    // Disable executing instructions from this page.
    NoExec = 64,
};

// operator| for PageAttr. Use to create combination of attributes.
// @param attr1: The first attr.
// @param attr2: The second attr.
// @return: The OR of attr1 and attr2.
PageAttr operator|(PageAttr const& attr1, PageAttr const& attr2);

// operator& for PageAttr. Used to test that a combination of attribute
// (computed using operator| for instance) contains a particular attribute.
// @param attr1: The first attr.
// @param attr2: The second attr.
// @return: true if attr1 and attr2 share at least one bit/flag, false
// otherwise.
bool operator&(PageAttr const& attr1, PageAttr const& attr2);

// Map a region of virtual memory to physical memory in the current address
// space. The region's size must be a multiple of page size.
// @param vaddrStart: The start virtual address of the region to be mapped.
// @param paddrStart: The start physical address at which the region should be
// mapped.
// @param pageAttr: Control the attribute of the mapping. All mapped pages will
// end up using those attributes.
// @param nPages: The size of the region in number of pages.
// @return: Returns an error if the mapping failed.
Err map(VirAddr const vaddrStart,
        PhyAddr const paddrStart,
        PageAttr const pageAttr,
        u64 const nPages);

// Unmap virtual pages from virtual memory. Attempting to unmap a page that is
// not currently mapped is a no-op.
// @param addrStart: The address to start unmapping from.
// @param nPages: The number of pages to unmap.
void unmap(VirAddr const addrStart, u64 const nPages);
}
