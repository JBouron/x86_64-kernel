// Definition of the Vector<T> type representing a dynamic size array.
#pragma once
#include <memory/malloc.hpp>
#include <util/subrange.hpp>

// Placement new operator. For some reason, g++ cannot find this operator if it
// is defined in malloc.cpp with other operators, hence putting it here. Note
// that using this operator is pretty dangerous as it does not care where the
// caller is trying to allocate. It is only meant to be used by the Vector
// implementation, no-one else should use this.
inline void *operator new(u64, void * placement) {
    return placement;
}

// A dynamic array inspired from c++'s std::vector.
// This type has the same properties as std::vector, namely:
//  - O(1) random access.
//  - O(1) insertion or removal of elements at the end of the vector
//  (amortized), O(n) in the worst case.
//  - Insertion or removal of elements that is linear in the distance to the end
//  of the vector.
// Not all functions of std::vector are re-implemented in this type, only the
// most useful ones, in order to keep things simple.
// Unlike std::vector, all accessor are asserting that the indices passed as
// argument are within the size of the vector.
template<typename T>
class Vector final {
public:
    // Create an empty vector. No dynamic allocation takes place until the first
    // element is inserted via insert() or pushBack();
    explicit Vector() : m_array(nullptr), m_size(0), m_capacity(0) {}

    // Create a vector with a start size. This allocate `size` object of type T
    // calling their default constructor.
    // @param size: The starting size of the vector.
    explicit Vector(u64 const size) : Vector() {
        growArray(size);
        for (u64 i(0); i < size; ++i) {
            constructAt(m_array + i);
        }
        m_size = size;
    }

    // Create a vector with a start size and set all elements to a given value.
    // Each element is constructed using the copy-constructor with `value` as
    // argument.
    // @param size: The starting size of the vector.
    // @param value: The value to copy into the first `start` elements of the
    // vector.
    explicit Vector(u64 const size, T const& value) : Vector() {
        growArray(size);
        for (u64 i(0); i < size; ++i) {
            constructAt(m_array + i, value);
        }
        m_size = size;
    }

    // Create a copy of a vector.
    // @param other: The vector to copy.
    Vector(Vector const& other) : Vector() {
        u64 const size(other.size());
        if (size) {
            growArray(size);
            for (u64 i(0); i < size; ++i) {
                constructAt(m_array + i, other[i]);
            }
            m_size = size;
        }
    }

    // Destroy the vector. This calls the destructor on all elements of the
    // vector.
    ~Vector() {
        if (!!m_array) {
            for (u64 i(0); i < m_size; ++i) {
                m_array[i].~T();
            }
            HeapAlloc::free(m_array);
        }
    }

    // Copy the content of another vector into this vector.
    // @param other: The vector to copy the content from.
    void operator=(Vector const& other) {
        // This may not be the most efficient, but this is clearly the easier
        // way to handle assignment.
        clear();
        for (T const& elem : other) {
            pushBack(elem);
        }
    }

    // Compare the content of this vector against another.
    // @param other: The other vector to compare against.
    // @return: true if both vectors have the same size and if all elements are
    // equal between the two vectors (and they appear in the same order).
    bool operator==(Vector const& other) const {
        if (size() != other.size()) {
            return false;
        } else {
            for (u64 i(0); i < size(); ++i) {
                if (m_array[i] != other.m_array[i]) {
                    return false;
                }
            }
            return true;
        }
    }

    // Access an element in the vector. This function asserts that the access is
    // within the limits of the vector.
    // @param index: The index of the element to access.
    // @return: A const reference to the element.
    T const& operator[](u64 const index) const {
        ASSERT(index < m_size);
        return m_array[index];
    }

    // Access an element in the vector. This function asserts that the access is
    // within the limits of the vector.
    // @param index: The index of the element to access.
    // @return: A reference to the element.
    T& operator[](u64 const index) {
        ASSERT(index < m_size);
        return m_array[index];
    }

    // operator[] overloads using a SubRange as an index.
    template<typename Impl, u64 _Min, u64 _Max, u64 _Default>
    T const& operator[](SubRange<Impl,_Min,_Max,_Default> const& index) const {
        return operator[](index.raw());
    }
    template<typename Impl, u64 _Min, u64 _Max, u64 _Default>
    T& operator[](SubRange<Impl,_Min,_Max,_Default> const& index) {
        return operator[](index.raw());
    }

    // Return the size of the vector.
    // @return: The number of elements in the vector.
    u64 size() const {
        return m_size;
    }

    // Check if the vector is empty.
    // @return: true if the vector does not contain any elements, false
    // otherwise.
    bool empty() const {
        return !m_size;
    }

    // Clear the content of this vector. This destroys all elements of the
    // vector.
    void clear() {
        while (m_size) {
            popBack();
        }
    }

    // Get the capacity of the vector, that is the maximum number of elements
    // this vector can hold before a reallocation occurs.
    // @return: The vector's capacity.
    u64 capacity() const {
        return m_capacity;
    }

    // Basic iterator type for a Vector<T>. Again only the most important/useful
    // method are implemented.
    // The template param U controls whether or not this iterator is a non-const
    // iterator (U = T) or a const iterator (U = T const).
    template<typename U>
    class Iterator {
    public:
        Iterator(U* start) : m_curr(start) {}
        bool operator==(Iterator const& other) const = default;

        // Access the element pointed by this iterator.
        // @return: A reference to the pointed element. This reference is const
        // for const iterators, non-const otherwise.
        U& operator*() {
            return *m_curr;
        }

        // Move this iterator to the next element.
        // @return: this.
        Iterator& operator++() {
            m_curr += 1;
            return *this;
        }
    private:
        // Currently pointed element.
        U* m_curr;
    };

    // Get a non-const iterator pointing to the first element of this vector.
    // @return: A non-const iterator.
    Iterator<T> begin() {
        return Iterator<T>(m_array);
    }

    // Get a non-const iterator pointing to one past the last element of this
    // vector.
    // @return: A non-const iterator.
    Iterator<T> end() {
        return Iterator<T>(m_array + m_size);
    }

    // Get a const iterator pointing to the first element of this vector.
    // @return: A const iterator.
    Iterator<T const> begin() const {
        return Iterator<T const>(m_array);
    }

    // Get a const iterator pointing to one past the last element of this
    // vector.
    // @return: A const iterator.
    Iterator<T const> end() const {
        return Iterator<T const>(m_array + m_size);
    }

    // Add an element to the end of the vector. The element is _copied_ to the
    // vector using the copy constructor. If the vector reaches maximum capacity
    // the underlying array is re-allocated and existing elements copied to the
    // new array (again using the copy constructor).
    // @param value: The value to insert.
    void pushBack(T const& value) {
        if (m_size == m_capacity) {
            // We reached the max capacity of the current array, need to
            // re-allocate it.
            growArray(!!m_capacity ? m_capacity * 2 : 8);
        }
        constructAt(m_array + m_size, value);
        m_size++;
    }

    // Remove the last element from the vector.
    void popBack() {
        ASSERT(!!m_size);
        m_array[m_size-1].~T();
        m_size--;
    }

    // Insert a value in the vector at the given index. All elements after that
    // index are "shifted" to the right using the assignment operator. The
    // insertion itself is also performed using the assignment operator.
    // @param index: The index at which to make the insertion.
    // @param value: The value to insert.
    void insert(u64 const index, T const& value) {
        // We allow inserting at the end by specifying an index that is 1 past
        // the last element.
        ASSERT(index <= m_size);
        if (index == m_size) {
            // Special case: The insertion is at the end of the vector, use
            // pushBack in this case.
            pushBack(value);
            return;
        }
        if (m_size == m_capacity) {
            // We reached the max capacity of the current array, need to
            // re-allocate it.
            growArray(!!m_capacity ? m_capacity * 2 : 8);
        }

        // At this point the vector must have at least one element because
        // otherwise m_size == 0 and only insert(0, value) is allowed, which is
        // handled by a pushBack.
        ASSERT(!!m_size);

        // Shift all elements "to the right" starting from the last element down
        // to the element at index `index`:
        // The last elements has a special handling since it is shifted at an
        // index that is not live, we need to use the copy constructor in this
        // case.
        constructAt(m_array + m_size, m_array[m_size-1]);
        // Other elements are shifted by using the assigment operator.
        for (u64 i(m_size - 1); i > index; --i) {
            m_array[i] = m_array[i-1];
        }
        // Now that the element at index `index` has been copied to `index+1`,
        // we can assign it to the new value.
        m_array[index] = value;
        m_size++;
    }

    // Remove the element at the given index. All elements at the right of this
    // index are then "shifted" to the left using the assignment operator.
    // @param index: The index of the element to remove from the vector.
    void erase(u64 const index) {
        ASSERT(index < m_size);
        for (u64 i(index); i < m_size - 1; ++i) {
            m_array[i] = m_array[i+1];
        }
        popBack();
    }

private:
    // The underlying array holding the elements.
    T* m_array;
    // The number of elements in the vector. This is <= the capacity of the
    // array m_array.
    u64 m_size;
    // The maximum capacity of the array m_array in number of elements.
    u64 m_capacity;
    // The following invariants hold:
    //  - If m_array is not-null then m_size > 0 and vice versa.
    //  - If m_array is null then m_size == 0.
    //  - m_size <= m_capacity.

    // Construct an object at the desired address. This is a wrapper around the
    // placement new operator that avoids cluttering the code with
    // reinterpret_casts everywhere.
    template<typename U, typename... Args>
    U* constructAt(U * ptr, Args&&... args) {
        return new (reinterpret_cast<void*>(ptr)) U(args...);
    }

    // Grow the underlying array to a new capacity. The array is reallocated and
    // all elements copied into the new array using T's copy constructor.
    // Destructors are then called on all elements of the old array and the
    // latter is deallocated.
    // @param newCapacity: The capacity to use for the new array. This must be
    // >= the current capacity.
    void growArray(u64 const newCapacity) {
        ASSERT(newCapacity >= m_capacity);
        Res<void*> const allocRes(HeapAlloc::malloc(newCapacity * sizeof(T)));
        // FIXME: We don't really have a choice other than asserting the
        // allocation goes well here. We have no way to communicate errors to
        // the caller.
        ASSERT(allocRes.ok());
        T* const newArray(reinterpret_cast<T*>(allocRes.value()));
        for (u64 i(0); i < m_size; ++i) {
            // Construct the element at index i in the new array using the copy
            // constructor directly instead of the assignment operator (T might
            // not have a default constructor!).
            constructAt(newArray + i, m_array[i]);
            // Destruct the copied element from the previous array.
            m_array[i].~T();
        }
        if (!!m_array) {
            HeapAlloc::free(m_array);
        }
        m_array = newArray;
        m_capacity = newCapacity;
    }
};
