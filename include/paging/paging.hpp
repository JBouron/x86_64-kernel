// Everything related to paging.

#pragma once
#include <util/util.hpp>
#include <bootstruct.hpp>

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
}
