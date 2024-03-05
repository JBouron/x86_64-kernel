// Implementation of Queue<T>.
#pragma once

#include <datastruct/list.hpp>

// Generic queue/FIFO data structure holding values of type T. Provides O(1)
// enqueue and dequeue operations.
template<typename T>
class Queue {
public:
    // Construct a default, empty queue. This is guaranteed not to allocate
    // memory in the heap, making it safe to use for global values.
    Queue() = default;

    // Enqueue a value in the queue. This creates a copy of this value in the
    // queue.
    // @param value: The value to enqueue.
    void enqueue(T const& value) {
        m_list.pushBack(value);
    }

    // Dequeue the value at the head of the queue and return it.
    // @return: A copy of the value.
    T dequeue() {
        return m_list.popFront();
    }

    // Get a reference to the value at the head of the queue. Const and
    // non-const version. These functions do _not_ dequeue the value from the
    // queue.
    T& front() {
        return m_list.front();
    }
    T const& front() const {
        return m_list.front();
    }

    // Get a reference to the value at the back of the queue. Const and
    // non-const version. These functions do _not_ dequeue the value from the
    // queue.
    T& back() {
        return m_list.back();
    }
    T const& back() const {
        return m_list.back();
    }

    // Get the size of the queue, ie. the number of elements currently enqueued.
    // FIXME: This is a O(N) operation due to List<T>::size() being O(N).
    u64 size() const {
        return m_list.size();
    }

    // Check if this queue is empty, ie. it does not contain any elements.
    bool empty() const {
        return !size();
    }

    // Remove all elements from the queue.
    void clear() {
        m_list.clear();
    }

private:
    // The underlying linked list holding the enqueued objects.
    List<T> m_list;
};
