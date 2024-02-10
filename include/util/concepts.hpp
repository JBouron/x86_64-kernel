// Collection of concepts and other useful templates.
#pragma once

struct TrueType { static constexpr bool value = true; };
struct FalseType { static constexpr bool value = false; };

template<typename U, typename V>
struct _SameAs : FalseType {};

template<typename U>
struct _SameAs<U, U> : TrueType {};

// Evaluates to true if U and V are the same type, false otherwise.
template<typename U, typename V>
inline constexpr bool SameAs = _SameAs<U, V>::value;
