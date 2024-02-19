// Smart pointer declaration.
#pragma once

#include <concurrency/atomic.hpp>
#include <selftests/selftests.hpp>
#include <util/assert.hpp>

// A smart pointer to a object of type T allocated on the heap. A Ptr<T> keeps
// track of the reference count of the object it points to. Copying a Ptr<T>
// creates a _new_ reference to that object, increasing the reference count.
// Destroying a Ptr<T> removes a reference to that object, if this was the last
// Ptr<T> referring to this object, the latter is de-allocated.
// Reference counting is implemented as an Atomic<u64> and is thread-safe.
template<typename T>
class Ptr {
public:
    // Dynamically allocate an object of type T.
    // @param args: The constructor parameters.
    // @return: A smart pointer to the new allocated object.
    template<typename... Args>
    static Ptr New(Args &&... args) {
        return new T(args...);
    }

    // Create a null smart pointer.
    Ptr() : Ptr(nullptr) {}

    // Copy a smart pointer. This create a new reference to the pointed-to
    // object.
    // @param other: The pointer to copy.
    Ptr(Ptr const& other) : m_ptr(other.m_ptr), m_refCount(other.m_refCount) {
        (*m_refCount)++;
    }

    // Copy a smart pointer of a subtype of T. This create a new reference to
    // the pointed U object.
    // @param other: The pointer to copy.
    template<typename U>
    Ptr(Ptr<U> const& other): m_ptr(other.m_ptr), m_refCount(other.m_refCount) {
        (*m_refCount)++;
    }

    // Destroy a smart pointer. If this was the last reference to the pointed
    // object, the object is de-allocated.
    ~Ptr() {
        reset();
    }

    // Assign this Ptr<T> to another Ptr<T>. This create a new reference to the
    // object pointed by `other` while deleting the reference to the current
    // object. If this Ptr<T> was the last reference to its object, it is
    // de-allocated.
    // @param other: The Ptr<T> to copy from.
    // @return: A reference to this Ptr<T>.
    Ptr& operator=(Ptr const& other) {
        reset();
        m_ptr = other.m_ptr;
        m_refCount = other.m_refCount;
        (*m_refCount)++;
        return *this;
    }

    // Assign this Ptr<T> to a Ptr<U> where U is a sub-type of T. This create a
    // new reference to the object pointed by `other` while deleting the
    // reference to the current object. If this Ptr<T> was the last reference to
    // its object, it is de-allocated.
    // @param other: The Ptr<U> to copy from.
    // @return: A reference to this Ptr<T>.
    template<typename U>
    Ptr& operator=(Ptr<U> const& other) {
        reset();
        m_ptr = other.m_ptr;
        m_refCount = other.m_refCount;
        (*m_refCount)++;
        return *this;
    }

    // Get the number of references to the pointer-to object.
    // @return: The current ref-count.
    u64 refCount() const {
        return m_refCount->read();
    }

    T& operator*() const {
        ASSERT(!!m_ptr && !!m_refCount->read());
        return *m_ptr;
    }

    T* operator->() const {
        ASSERT(!!m_ptr && !!m_refCount->read());
        return m_ptr;
    }

    // Check if the pointer is a null pointer.
    // @return: true if this is a null pointer, false otherwise.
    operator bool() const {
        return !!m_ptr;
    }

    // Return a raw pointer to the referenced object.
    T* raw() const {
        ASSERT(!!m_ptr && !!m_refCount->read());
        return m_ptr;
    }

private:
    Ptr(T* ptr) : m_ptr(ptr), m_refCount(new Atomic<u64>(1)) {}

    // Reset this Ptr<T> by decrementing the ref count and de-allocating the
    // referenced object if this was the last reference. m_ptr is set to nullptr
    // after this call.
    void reset() {
        if (!--(*m_refCount) && !!m_ptr) {
            delete m_ptr;
        }
        m_ptr = nullptr;
    }

    T* m_ptr;
    Atomic<u64>* m_refCount;

    // Needed to be able to access m_ptr and m_refCount in Ptr(Ptr<U>) and
    // operator=(Ptr<U>).
    template<typename> friend class Ptr;
};

namespace SmartPtr {
// Run the tests for the Ptr<T> type.
void Test(SelfTests::TestRunner& runner);
}
