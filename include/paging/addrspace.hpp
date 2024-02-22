// Types and definition related to address spaces.
#pragma once

#include <util/addr.hpp>
#include <util/ptr.hpp>
#include <util/result.hpp>

namespace Paging {

// Represents an address space. This RAII-style object takes care of allocating
// page-tables upon init and de-allocating them upon deletion. All address space
// share the same kernel mapping, ie. the second half of their PML4 entries are
// identical.
class AddrSpace {
public:
    // Create a new address space. The new address space shares the mapping of
    // kernel addresses used by the current address space. The user addresses
    // are un-mapped.
    // @return: A Ptr to the new AddrSpace or an error if any.
    static Res<Ptr<AddrSpace>> New();

    // De-allocate part of the page-table structure used by this AddrSpace that
    // is not shared with other AddrSpace (ie. the user mapping).
    ~AddrSpace();

    // Get the address of the PML4 associated with this address space.
    // @return: The physical address of the PML4.
    PhyAddr pml4Address() const;

    // Switch from the current address space to another one.
    // @param to: The address space to switch to.
    static void switchAddrSpace(Ptr<AddrSpace> const& to);
    // This overload is mostly meant for testing.
    static void switchAddrSpace(PhyAddr const& pml4);

private:
    // Create an AddrSpace.
    // @param pml4: Physical address of the top-level page table.
    AddrSpace(PhyAddr const pml4);
    friend Ptr<AddrSpace>;

    // AddrSpaces are not copyable. This is because they own the page-tables
    // under m_pml4Address.
    AddrSpace(AddrSpace const& other) = delete;
    AddrSpace& operator=(AddrSpace const& other) = delete;
    // For now we don't need the move constructor / assignment either.
    AddrSpace(AddrSpace && other) = delete;
    AddrSpace& operator=(AddrSpace && other) = delete;

    PhyAddr m_pml4Address;
};
}
