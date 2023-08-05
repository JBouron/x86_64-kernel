// Types for virtual and physical addresses so that it is impossible to
// accidently assign a virtual address with a physical one and vice-versa.
#pragma once

#include <util/ints.hpp>
#include <selftests/selftests.hpp>

// The size of a page in bytes. For now this kernel only supports the default
// size of 4096 bytes pages.
static constexpr u64 PAGE_SIZE = 0x1000;

// Base class for VirAddr and PhyAddr. This is essentially a wrapper around a
// u64. This type is not constructible outside of VirAddr and PhyAddr.
// The template param Impl controls the type used/returned by the operators.
template<typename Impl>
class AddrBase {
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

    // Comparison operators between the _same_ types, e.g. comparing a VirAddr
    // and a PhyAddr is not possible/allowed.
#define CMP_OPERATOR(op) \
    bool operator op (Impl const& o) const { return m_addr op o.m_addr; }

    CMP_OPERATOR(!=);
    CMP_OPERATOR(==);
    CMP_OPERATOR(<);
    CMP_OPERATOR(<=);
    CMP_OPERATOR(>);
    CMP_OPERATOR(>=);
#undef CMP_OPERATOR

    // Add or subtract an offset from an address.
    // @param offset: The offset to add/subtract.
    // @return: The resulting address, same type as the original address.
    Impl operator+(u64 const offset) const { return m_addr + offset; }
    Impl operator-(u64 const offset) const { return m_addr - offset; }

    // Compute the difference between two addresses.
    // @param other: The other address.
    // @return: The difference in number of bytes.
    u64 operator-(Impl const& other) const { return m_addr - other.m_addr; }

    // Check if this address is non-NULL.
    // @return: true if this address is non-NULL, false otherwise.
    explicit operator bool() const {
        return !!m_addr;
    }

protected:
    // Create a default Addr, initialized to address 0x0.
    AddrBase() : m_addr(0) {}

    // Create an AddrBase.
    // @param addr: The initial value of the AddrBase.
    AddrBase(u64 const addr) : m_addr(addr) {}

    // The value of the address.
    u64 m_addr;
};


class PhyAddr;

// Value type describing a virtual address.
class VirAddr : public AddrBase<VirAddr> {
public:
    // Create a VirtAddr initialized to 0x0.
    VirAddr() : AddrBase<VirAddr>() {}

    // Create a VirtAddr.
    // @param addr: The initial value of the address.
    VirAddr(u64 const addr) : AddrBase<VirAddr>(addr) {}

    // Cast this address to a pointer to T.
    // @return: The pointer to T equivalent to this address.
    template<typename T>
    T* ptr() const { return reinterpret_cast<T*>(m_addr); }
};
static_assert(sizeof(VirAddr) == sizeof(u64));


// Value type describing a physical address.
class PhyAddr : public AddrBase<PhyAddr> {
public:
    // Create a PhyAddr initialized to 0x0.
    PhyAddr() : AddrBase<PhyAddr>() {}

    // Create a PhyAddr.
    // @param addr: The initial value of the address.
    PhyAddr(u64 const addr) : AddrBase<PhyAddr>(addr) {}
};
static_assert(sizeof(PhyAddr) == sizeof(u64));


// Some static asserts on how VirAddr and PhyAddr can be used. Most importantly
// those asserts make sure that we cannot mix VirAddrs and PhyAddrs.
// Test #1: Cannot construct a VirAddr from a PhyAddr and vice-versa.
template<typename U, typename V>
concept Constructible = requires(V const& v) { U(v); };
static_assert(!Constructible<VirAddr, PhyAddr>);
static_assert(Constructible<VirAddr, VirAddr>);
static_assert(!Constructible<PhyAddr, VirAddr>);
static_assert(Constructible<PhyAddr, PhyAddr>);

// Test #2: Cannot compare a VirAddr with a PhyAddr and vice-versa.
template<typename U, typename V>
concept ComparableNe = requires(U u, V v) { u != v; };
template<typename U, typename V>
concept ComparableEq = requires(U u, V v) { u == v; };
template<typename U, typename V>
concept ComparableLt = requires(U u, V v) { u < v; };
template<typename U, typename V>
concept ComparableLe = requires(U u, V v) { u <= v; };
template<typename U, typename V>
concept ComparableGt = requires(U u, V v) { u > v; };
template<typename U, typename V>
concept ComparableGe = requires(U u, V v) { u >= v; };
static_assert(!ComparableNe<VirAddr, PhyAddr>);
static_assert(!ComparableEq<VirAddr, PhyAddr>);
static_assert(!ComparableLt<VirAddr, PhyAddr>);
static_assert(!ComparableLe<VirAddr, PhyAddr>);
static_assert(!ComparableGt<VirAddr, PhyAddr>);
static_assert(!ComparableGe<VirAddr, PhyAddr>);
static_assert(!ComparableNe<PhyAddr, VirAddr>);
static_assert(!ComparableEq<PhyAddr, VirAddr>);
static_assert(!ComparableLt<PhyAddr, VirAddr>);
static_assert(!ComparableLe<PhyAddr, VirAddr>);
static_assert(!ComparableGt<PhyAddr, VirAddr>);
static_assert(!ComparableGe<PhyAddr, VirAddr>);
// Can compare two VirAddr or PhyAddr.
static_assert(ComparableNe<VirAddr, VirAddr>);
static_assert(ComparableEq<VirAddr, VirAddr>);
static_assert(ComparableLt<VirAddr, VirAddr>);
static_assert(ComparableLe<VirAddr, VirAddr>);
static_assert(ComparableGt<VirAddr, VirAddr>);
static_assert(ComparableGe<VirAddr, VirAddr>);
static_assert(ComparableNe<PhyAddr, PhyAddr>);
static_assert(ComparableEq<PhyAddr, PhyAddr>);
static_assert(ComparableLt<PhyAddr, PhyAddr>);
static_assert(ComparableLe<PhyAddr, PhyAddr>);
static_assert(ComparableGt<PhyAddr, PhyAddr>);
static_assert(ComparableGe<PhyAddr, PhyAddr>);

// Test #3: Cannot subtract a VirAddr from a PhyAddr and vice versa.
template<typename U, typename V>
concept Subtractable = requires(U u, V v) { u - v; };
static_assert(!Subtractable<VirAddr, PhyAddr>);
static_assert(!Subtractable<PhyAddr, VirAddr>);
// Can subtract two VirAddr or two PhyAddr.
static_assert(Subtractable<VirAddr, VirAddr>);
static_assert(Subtractable<PhyAddr, PhyAddr>);
