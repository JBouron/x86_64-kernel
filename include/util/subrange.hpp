// Definition of the SubRange type.
#pragma once
#include <util/assert.hpp>
#include <util/addr.hpp>

// Inspired by Pascal, a SubRange is type capable of holding only values
// contained in a certain sub-range of u64. Any value outside of the range [Min;
// Max] is rejected by the constructor by a panic.
// SubRange is NOT a collection, it is merely a wrapper around the underlying
// type which performs range checks.
// SubRange should not be used as-is but instead should be used as a base type
// for a new type, for example: class Ring : SubRange<Ring, 0, 3> {}; The Impl
// template parameter is used to make sure that the comparison operators and
// copy constructor only take parameters of type Impl and not
// SubRange<Impl,Min,Max>, this means that even if A and B are deriving from
// SubType with same Min and Max, they cannot be compared or assigned (assuming
// they set their Impl to A and B respectively).
// SubType does not provide arithmetic operators such as +, -, *, ... because it
// is expected that most types deriving from SubRange don't have well-defined
// arithmetic operators (what does it mean to divide two values of type Ring??)
// Nevertheless, if a particular type requires those operator, it can perform
// those operations on raw().
// The postfix ++ operator is provided however, as it makes it easier to use a
// SubRange (or subtype) as a loop iterator.
// Template params:
// @param Impl: The type of the type deriving from this one. Copy constructor,
// assignment and comparison operators are all operating on this type.
// @param _Min: The minimum allowed value by this type.
// @param _Max: The maximum allowed value by this type.
// @param _Default: The default value for the default constructor. If not
// specified it is set to _Min.
template<typename Impl,
         u64 _Min,
         u64 _Max,
         u64 _Default = _Min>
requires (_Min < _Max && _Min <= _Default && _Default <= _Max)
class SubRange {
public:
    // The underlying type used to hold the value of this instance of SubRange.
    // FIXME: Eventually this should be optimized so that it uses the smallest
    // uint possible depending on the Max.
    using UnderlyingType = u64;

    // Minimum, Maximum and Default values for this type.
    static constexpr UnderlyingType Min = _Min;
    static constexpr UnderlyingType Max = _Max;
    static constexpr UnderlyingType Default = _Default;

    // Default constructor. The resulting instance holds the value of Default.
    SubRange() : SubRange(Default) {}

    // Construct an instance of SubRange containing a value. This value is
    // checked against the bound Min and Max, and, if it does not fall between
    // those, a Panic is raised.
    // @param value: The value to be stored by this instance.
    SubRange(UnderlyingType const value) : m_value(value) {
        ASSERT(Min <= value && value <= Max);
    }

    // "Copy constructor" with another Impl. This constructor cannot panic as
    // the value contained by other has already been validated.
    // @param other: The Impl to copy the value from.
    SubRange(Impl const& other) : SubRange(other.m_value) {}

    // Assignment operator. This cannot panic as the other's value has already
    // been validated.
    // @param other: The Impl to assign from.
    Impl& operator=(Impl const& other) {
        m_value = other.m_value;
        return *static_cast<Impl*>(this);
    }

    // FIXME: It would be good to have operator=(UnderlyingType const), however
    // it seems that defining such an operator here does not allow deriving type
    // to use it. So for now, assignment must be done through copy constructor.

    // Get the raw value contained in this instance.
    // @return: The raw value of type UnderlyingType.
    UnderlyingType raw() const {
        return m_value;
    }

    // Comparison operators. Against Impl object and against underlying types.
#define CMP_OPERATORS(op)                                                   \
    bool operator op (Impl const& o) const { return m_value op o.m_value; } \
    bool operator op (UnderlyingType const& o) const { return m_value op o; }

    CMP_OPERATORS(!=);
    CMP_OPERATORS(==);
    CMP_OPERATORS(<);
    CMP_OPERATORS(<=);
    CMP_OPERATORS(>);
    CMP_OPERATORS(>=);
#undef CMP_OPERATOR

    // Increment the value contained in this instance. This operator raises a
    // PANIC if the value becomes > Max.
    Impl& operator++() {
        u64 const newValue(m_value + 1);
        ASSERT(Min <= newValue && newValue <= Max);
        m_value = newValue;
        return *static_cast<Impl*>(this);
    }

protected:
    // The stored value of this instance.
    UnderlyingType m_value;
};

// Below are static_asserts asserting how SubRange can and can't be used.
class A : public SubRange<A, 0, 123> {};
class B : public SubRange<B, 0, 123> {};
class C : public SubRange<C, 256, 1024, 500> {};

// Check Min, Max and Default.
static_assert(A::Min == 0 && A::Max == 123 && A::Default == 0);
static_assert(B::Min == 0 && B::Max == 123 && B::Default == 0);
static_assert(C::Min == 256 && C::Max == 1024 && C::Default == 500);

// Check that we have zero storage overhead.
static_assert(sizeof(A) == sizeof(A::UnderlyingType));
static_assert(sizeof(B) == sizeof(B::UnderlyingType));
static_assert(sizeof(C) == sizeof(C::UnderlyingType));

// Check if a U can be constructed from a V.
template<typename U, typename V>
concept CopyConstructible = requires (V v) { U(v); };
static_assert(CopyConstructible<A, A>);
static_assert(!CopyConstructible<A, B>);
static_assert(!CopyConstructible<A, C>);

// Check if a U can be assigned a V.
template<typename U, typename V>
concept Assignable = requires (U u, V v) { u = v; };
static_assert(Assignable<A, A>);
static_assert(!Assignable<A, B>);
static_assert(!Assignable<A, C>);

// Check comparison operators.
// FIXME: We are reusing concepts defined in addr.hpp. We should define those
// concepts in their own header instead.
static_assert(ComparableNe<A, A>);
static_assert(ComparableEq<A, A>);
static_assert(ComparableLt<A, A>);
static_assert(ComparableLe<A, A>);
static_assert(ComparableGt<A, A>);
static_assert(ComparableGe<A, A>);
static_assert(!ComparableNe<A, B>);
static_assert(!ComparableEq<A, B>);
static_assert(!ComparableLt<A, B>);
static_assert(!ComparableLe<A, B>);
static_assert(!ComparableGt<A, B>);
static_assert(!ComparableGe<A, B>);
static_assert(!ComparableNe<A, C>);
static_assert(!ComparableEq<A, C>);
static_assert(!ComparableLt<A, C>);
static_assert(!ComparableLe<A, C>);
static_assert(!ComparableGt<A, C>);
static_assert(!ComparableGe<A, C>);
// Comparison with underlying type.
static_assert(ComparableNe<A, A::UnderlyingType>);
static_assert(ComparableEq<A, A::UnderlyingType>);
static_assert(ComparableLt<A, A::UnderlyingType>);
static_assert(ComparableLe<A, A::UnderlyingType>);
static_assert(ComparableGt<A, A::UnderlyingType>);
static_assert(ComparableGe<A, A::UnderlyingType>);

template<typename U>
concept PreIncrementable = requires (U u) { ++u; };
static_assert(PreIncrementable<A>);
static_assert(PreIncrementable<B>);
static_assert(PreIncrementable<C>);
