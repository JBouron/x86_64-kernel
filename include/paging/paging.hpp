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

// Get the virtual address in the direct map corresponding to the given physical
// address.
// @param paddr: The physical address to translate.
// @return: The direct map address mapped to `paddr`.
VirAddr toVirAddr(PhyAddr const paddr);

// Initialize paging.
// This function creates the direct map.
void Init(BootStruct const& bootStruct);

// Run paging tests.
void Test(SelfTests::TestRunner& runner);

// Map a region of virtual memory to physical memory in the current address
// space. The region's size must be a multiple of page size.
// @param vaddrStart: The start virtual address of the region to be mapped.
// @param paddrStart: The start physical address at which the region should be
// mapped.
// @param nPages: The size of the region in number of pages.
// @return: Returns an error if the mapping failed.
Err map(VirAddr const vaddrStart, PhyAddr const paddrStart, u64 const nPages);
}
