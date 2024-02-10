// Definition of the Atomic<T> type.
#pragma once
#include <util/concepts.hpp>

// NOT TO BE USED DIRECTLY!
// Perform a fetch&add operation, atomically executes the following:
//  {
//      u64 oldVal = *ptr;
//      *ptr += add;
//      return oldVal;
//  }
// @param ptr: The address to atomically add into.
// @param add: The value to add to the atomic variable.
// @return: The value of *ptr _before_ the add.
extern "C" u64 _fetchAdd(u64 volatile* const ptr, u64 const add);

// NOT TO BE USED DIRECTLY!
// Perform a compare&exchange operation. Runs the following atomically:
//  {
//      if (*ptr == expected) {
//          *ptr = desired;
//          return true;
//      } else {
//          return false;
//      }
//  }
// @param expected: The value to compare against.
// @param desired: The value to store in this Atomic<T> if the current value
// equals the expected one.
// @return: true if the cmpxchg operation was successful, false otherwise.
extern "C" bool _cmpxchg(u64 volatile* const ptr, u64 const exp, u64 const to);

// Class representing an atomic value of type T. All operations on the
// underlying value are perfomed atomically. For now only unsigned integers are
// supported but the class can easily be extended to support other types.
// Heavily inspired from the std::atomic<T> template class.
template<typename T> requires (
    SameAs<T, u8> || SameAs<T, u16> || SameAs<T, u32> || SameAs<T, u64>)
class Atomic {
public:
    // Construct an atomic<T>, the default value is 0. Construction is not an
    // atomic operation.
    Atomic() : m_value(0) {}

    // Construct an atomic<T> with an initial value. Construction is not an
    // atomic operation.
    // @param initialValue: The initial value of the underlying atomic value.
    Atomic(T const& initialValue) : m_value(initialValue) {}

    // Atomic<T>'s cannot be copied or assigned to one another. For now there is
    // no good reason for this because we only support uints.
    Atomic(Atomic const& other) = delete;
    Atomic& operator=(Atomic const& other) volatile = delete;

    // Atomically read the current value stored in this Atomic<T>.
    // @return: The current value.
    T read() const volatile {
        return m_value;
    }

    // Atomically read the current value stored in this Atomic<T>.
    // @return: The current value.
    operator T() const volatile {
        return read();
    }

    // Atomically write a new value in this Atomic<T>.
    // @param newValue: The new value to assign the underlying value to.
    void write(T const& newValue) volatile {
        m_value = newValue;
    }

    // Atomically write a new value in this Atomic<T>.
    // @param newValue: The new value to assign the underlying value to.
    // @return: The new value.
    T operator=(T const& newValue) volatile {
        write(newValue);
        return newValue;
    }

    // Perform an atomic compare & exchange operation, that is executes the
    // following atomically:
    //  {
    //      if (value == expected) {
    //          value = desired;
    //          return true;
    //      } else {
    //          return false;
    //      }
    //  }
    // @param expected: The value to compare against.
    // @param desired: The value to store in this Atomic<T> if the current value
    // equals the expected one.
    // @return: true if the cmpxchg operation was successful, false otherwise.
    bool compareAndExchange(T const expected, T const desired) volatile {
        return _cmpxchg(&m_value, expected, desired);
    }

    // Atomically increment the stored value (pre-increment).
    // @return: The value of the atomic variable after the modification.
    T operator++() volatile {
        return _fetchAdd(&m_value, 1) + 1;
    }

    // Atomically decrement the stored value (pre-decrement).
    // @return: The value of the atomic variable after the modification.
    T operator--() volatile {
        return _fetchAdd(&m_value, -1) - 1;
    }

    // Atomically increment the stored value (post-increment).
    // @return: The value of the atomic variable before the modification.
    T operator++(int) volatile {
        return _fetchAdd(&m_value, 1);
    }

    // Atomically decrement the stored value (post-decrement).
    // @return: The value of the atomic variable before the modification.
    T operator--(int) volatile {
        return _fetchAdd(&m_value, -1);
    }

    // Atomically add a value to the stored variable.
    // @param val: The value to add to the variable.
    // @return: The new value of the variable after the modification.
    T operator+=(T const& val) volatile {
        return _fetchAdd(&m_value, val);
    }

    // Atomically add a value to the stored variable.
    // @param val: The value to add to the variable.
    // @return: The new value of the variable after the modification.
    T operator-=(T const& val) volatile {
        return _fetchAdd(&m_value, val);
    }

private:
    // FIXME: For now, all Atomic<T> are implemented as an atomic u64. This is
    // to keep things simple. Eventually we should support more types, if ever
    // needed.
    u64 m_value;
};
