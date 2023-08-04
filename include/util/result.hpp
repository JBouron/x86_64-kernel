// Definition of the Res<T> template class.
#pragma once

#include <util/panic.hpp>
#include <util/error.hpp>
#include <selftests/selftests.hpp>

// Wrapper class that either contain a result/value or an Error. Useful for
// functions that need to return either a value or an error, for instance
// alloc(), ...
// T and Error are contained in a union so the memory footprint of Res<T> is
// minimal, typically max(sizeof(Error), sizeof(T)) + 1 modulo any padding added
// by the compiler.
template<typename T>
class Res {
public:
    // Construct a default Res<T> containing a default value of T.
    Res() : m_isError(false), m_value() {}

    // Construct a Res<T> containing an error.
    Res(Error const err) : m_isError(true), m_error(err) {}

    // Construct a Res<T> containing a value copy initialized from the given
    // value.
    // @param value: The value to construct the contained value from (copy).
    Res(T const& value) : m_isError(false), m_value(value) {}

    // Construct a Res<T> containing a value by constructing the value in-place.
    // @param args...: The constructor parameters used to construct the value.
    template<typename... Args>
    Res(Args&&... args) : m_isError(false), m_value(args...) {}

    // Destroy a Res<T>. If this Res<T> contained a value, this value is
    // destroyed by calling its destructor.
    ~Res() {
        if (ok()) {
            m_value.~T();
        } else {
            // Typically not needed as Error is an enum for now, but this could
            // be useful in the future.
            m_error.~Error();
        }
    }

    // Check if this Res<T> contains a value, e.g. does not contain an error.
    // @return: true if this Res<T> contains a value, false otherwise (e.g.
    // contains an error).
    bool ok() const {
        return !m_isError;
    }

    // Shortcut for ok() when a Res<T> is used in a condition.
    // @return: true if this Res<T> contains a value, false otherwise.
    explicit operator bool() const {
        return ok();
    }

    // Get the error contained in this Res<T>. PANICs if this Res<T> does not
    // contain an error.
    // @return: The error contained in this Res<T>.
    Error error() const {
        if (ok()) {
            PANIC("Attempt to call error() on Res<T> with ok() == true");
        }
        return m_error;
    }

    // Get a reference to the value contained in this Res<T>. PANICs if this
    // Res<T> does not contain a value.
    // @return: A reference to the value contained in this Res<T>.
    T& value() {
        if (!ok()) {
            PANIC("Attempt to call value() on Res<T> with ok() == false");
        }
        return m_value;
    }

    // Get a const reference to the value contained in this Res<T>. PANICs if
    // this Res<T> does not contain a value.
    // @return: A const reference to the value contained in this Res<T>.
    T const& value() const {
        if (!ok()) {
            PANIC("Attempt to call value() on Res<T> with ok() == false");
        }
        return m_value;
    }

    // Get a reference to the value contained in this Res<T>. PANICs if this
    // Res<T> does not contain a value. This makes Res<T> acts as some sort of
    // pseudo-pointer.
    // @return: A reference to the value contained in this Res<T>.
    T& operator*() {
        if (!ok()) {
            PANIC("Attempt to call value() on Res<T> with ok() == false");
        }
        return m_value;
    }

    // Get a const reference to the value contained in this Res<T>. PANICs if
    // this Res<T> does not contain a value. This makes Res<T> acts as some sort
    // of pseudo-pointer.
    // @return: A const reference to the value contained in this Res<T>.
    T const& operator*() const {
        if (!ok()) {
            PANIC("Attempt to call operator*() on Res<T> with ok() == false");
        }
        return m_value;
    }

    // Get a pointer to the value contained in this Res<T>. PANICs if this
    // Res<T> does not contain a value. This makes Res<T> acts as some sort of
    // pseudo-pointer.
    // @return: A pointer to the value contained in this Res<T>.
    T* operator->() {
        if (!ok()) {
            PANIC("Attempt to call operator->() on Res<T> with ok() == false");
        }
        return &m_value;
    }

    // Get a const pointer to the value contained in this Res<T>. PANICs if
    // this Res<T> does not contain a value. This makes Res<T> acts as some sort
    // of pseudo-pointer.
    // @return: A const pointer to the value contained in this Res<T>.
    T const* operator->() const {
        if (!ok()) {
            PANIC("Attempt to call operator->() on Res<T> with ok() == false");
        }
        return &m_value;
    }

private:
    // true if this Res<T> contains an error, false otherwise.
    bool m_isError;
    union {
        Error m_error;
        T m_value;
    };
};

namespace Result {
// Run the tests for the Res<T> type.
void Test(SelfTests::TestRunner& runner);
}
