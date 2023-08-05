// Everything related to paging.

#pragma once
#include <util/util.hpp>
#include <util/err.hpp>
#include <bootstruct.hpp>
#include <selftests/selftests.hpp>

// The size of a page in bytes. For now this kernel only supports the default
// size of 4096 bytes pages.
static constexpr u64 PAGE_SIZE = 0x1000;

// Custom types for virtual and physical addresses so that it is impossible to
// accidently assign a virtual address with a physical one and vice-versa.
// FIXME: There is still some more work to be done on those types, like allowing
// adding two addresses of the same type etc...
// Forward declarations for the Addr base class, see below for real
// declarations.
class VirAddr;
class PhyAddr;

// Base class for VirAddr and PhyAddr. This is essentially a wrapper around a
// u64. This type is not constructible outside of VirAddr and PhyAddr.
class Addr {
public:
    // Convert to a u64.
    // @return: The u64 equivalent of this address.
    explicit operator u64() const { return m_addr; }

    // Convert to a u64.
    // @return: The u64 equivalent of this address.
    u64 raw() const { return m_addr; }

    // Check if the address is page aligned.
    // @return: true if the address is page aligned, false otherwise.
    bool isPageAligned() const { return !(m_addr & (PAGE_SIZE - 1)); }

private:
    // Declare VirAddr and PhyAddr as friends so only those can construct an
    // Addr.
    friend VirAddr;
    friend PhyAddr;

    // Create a default Addr, initialized to address 0x0.
    Addr() : m_addr(0) {}

    // Create an Addr.
    // @param addr: The initial value of the Addr.
    Addr(u64 const addr) : m_addr(addr) {}

    // The value of the address.
    u64 m_addr;
};


// Value type describing a virtual address.
class VirAddr : public Addr {
public:
    // Create a VirtAddr initialized to 0x0.
    VirAddr() : Addr() {}

    // Create a VirtAddr.
    // @param addr: The initial value of the address.
    VirAddr(u64 const addr) : Addr(addr) {}

    // Make sure it is illegal to create a VirtAddr from a PhyAddr.
    VirAddr(PhyAddr const& a) = delete;

    // Cast this address to a pointer to T.
    // @return: The pointer to T equivalent to this address.
    template<typename T>
    T* ptr() const { return reinterpret_cast<T*>(m_addr); }
};
static_assert(sizeof(VirAddr) == sizeof(u64));


// Value type describing a physical address.
class PhyAddr : public Addr {
public:
    // Create a PhyAddr initialized to 0x0.
    PhyAddr() : Addr() {}

    // Create a PhyAddr.
    // @param addr: The initial value of the address.
    PhyAddr(u64 const addr) : Addr(addr) {}

    // Make sure it is illegal to create a PhyAddr from a VirtAddr.
    PhyAddr(VirAddr const& a) = delete;
};
static_assert(sizeof(PhyAddr) == sizeof(u64));

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
