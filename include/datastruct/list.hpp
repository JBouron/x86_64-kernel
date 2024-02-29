// Doubly linked-list implementation.
#pragma once

#include <util/ptr.hpp>

// Generic doubly linked-list implementation holding values of type T. The
// implementation is somewhat inspired by std::list.
// Specifications:
//  - O(1) access, insertion and removal to the front and the back of the list.
//  - O(1) insertion and deletion in the middle of the list, if using an
//  iterator.
//  - No random access, need to use an iterator.
// The interface differs a bit from std::list when its more convient. For
// instance popFront() and popBack() return the popped value, at the cost of
// creating a copy, which is needed in 99% of the cases anyway.
template<typename T>
class List {
public:
    // Create an empty linked-list.
    List() = default;

    // Create a copy of a linked-list.
    // @param other: The linked-list to copy.
    List(List const& other) : List() {
        operator=(other);
    }

    // Empty this linked-list and copy the content of another.
    // @param other: The linked-list to copy.
    List& operator=(List const& other) {
        // For now we clear the list and add each element of the source list
        // one-by-one. A more optimized version would be to assign the existing
        // elements of the destination list and then add the remaining elements
        // to it, this would avoid needlessly destroying and constructing
        // object.
        // For now this is fine the way it is, we are not using this operator
        // much anyway and doing it this way avoids a couple of edge-cases and
        // test-cases (ie source list shorter, dest list shorter, ...).
        clear();
        for (T const& elem : other) {
            pushBack(elem);
        }
        return *this;
    }

    // No need for move operations yet.
    List(List && other) = delete;
    void operator=(List && other) = delete;

    // Delete all elements at destruction time.
    ~List() {
        clear();
    }

    // Compare this linked list against another.
    // @param other: The other linked list to compare against.
    // @return: true if both list have the same size and each element of this
    // list compares equals with the element in `other` at the same position.
    bool operator==(List const& other) const {
        List::IterConst thisIte(begin());
        List::IterConst thisIteEnd(end());
        List::IterConst otherIte(other.begin());
        List::IterConst otherIteEnd(other.end());
        while (thisIte != thisIteEnd && otherIte != otherIteEnd) {
            if (*thisIte != *otherIte) {
                return false;
            }
            ++thisIte;
            ++otherIte;
        }
        return thisIte == thisIteEnd && otherIte == otherIteEnd;
    }

    // Get the number of elements contained in this list. O(N) complexity.
    u64 size() const {
        Node const* ptr(m_head.next);
        u64 res(0);
        while (ptr != &m_head) {
            res++;
            ptr = ptr->next;
        }
        return res;
    }

    // Check if the list is empty. O(1) complexity.
    bool empty() const {
        return m_head.next == &m_head;
    }

    // Remove all elements from this List. O(N) complexity.
    void clear() {
        while (!empty()) {
            delete m_head.next;
        }
    }

    // Add an element to the front of the list. The new value is
    // copy-constructed. O(1) complexity.
    // @param value: The value to add.
    void pushFront(T const& value) {
        Node* const node(new Node(&m_head, m_head.next, value));
        ASSERT(!!node);
    }

    // Add an element to the back of the list. The new value is
    // copy-constructed. O(1) complexity.
    // @param value: The value to add.
    void pushBack(T const& value) {
        Node* const node(new Node(m_head.prev, &m_head, value));
        ASSERT(!!node);
    }

    // Remove the first element from the list and return its value. O(1)
    // complexity.
    // @return: A _copy_ of the first element from the list.
    T popFront() {
        ASSERT(!empty());
        T const first(m_head.next->value);
        delete m_head.next;
        return first;
    }

    // Remove the last element from the list and return its value. O(1)
    // complexity.
    // @return: A _copy_ of the last element from the list.
    T popBack() {
        ASSERT(!empty());
        T const last(m_head.prev->value);
        delete m_head.prev;
        return last;
    }

    // Forward declaration needed to define the Iterator class.
private:
    struct Node;
public:

    // Linked list iterator mostly for for(...) loops.
    class Iter {
    public:
        Iter(Node* const node) : m_node(node) {}
        bool operator==(Iter const& other) const = default;

        T& operator*() {
            return m_node->value;
        }

        Iter& operator++() {
            m_node = m_node->next;
            return *this;
        }

        // Remove from the list the element pointed by the iterator. The
        // iterator remains valid after this operation and advances to the
        // element following the deleted element.
        void erase() {
            // If this assert fails then we are trying to call erase() on an
            // iterator that is == end().
            ASSERT(m_node->hasValue);
            Node* const toDel(m_node);
            m_node = m_node->next;
            delete toDel;
        }

    private:
        // The node currently pointed by the iterator. For the end() iterator
        // this is the m_head of the parent linked list.
        Node* m_node;
    };

    class IterConst {
    public:
        IterConst(Node const * const node) : m_node(node) {}
        bool operator==(IterConst const& other) const = default;

        T const& operator*() const {
            return m_node->value;
        }

        IterConst& operator++() {
            m_node = m_node->next;
            return *this;
        }

    private:
        // The node currently pointed by the iterator. For the end() iterator
        // this is the m_head of the parent linked list.
        Node const* m_node;
    };

    // Usual STL-like iterators.
    Iter begin() {
        return Iter(m_head.next);
    }

    Iter end() {
        return Iter(&m_head);
    }

    IterConst begin() const {
        return IterConst(m_head.next);
    }

    IterConst end() const {
        return IterConst(&m_head);
    }

    // Get a reference to the first value in the list. O(1) complexity.
    // @return: A mutable reference to the first element of the list.
    T& front() {
        ASSERT(!empty());
        return m_head.next->value;
    }

    // Get a reference to the first value in the list. O(1) complexity.
    // @return: A non-mutable reference to the first element of the list.
    T const& front() const {
        ASSERT(!empty());
        return m_head.next->value;
    }

    // Get a reference to the last value in the list. O(1) complexity.
    // @return: A mutable reference to the last element of the list.
    T& back() {
        ASSERT(!empty());
        return m_head.prev->value;
    }

    // Get a reference to the last value in the list. O(1) complexity.
    // @return: A non-mutable reference to the last element of the list.
    T const& back() const {
        ASSERT(!empty());
        return m_head.prev->value;
    }

private:
    // A node in the linked-list.
    struct Node {
        // Default construct a node. The resulting node's prev and next points
        // to itself.
        Node() : prev(this), next(this), hasValue(false) {}

        // Construct a node and insert it between the nodes `prev` and `next`.
        Node(Node* const prev, Node* const next, T const& value) :
            prev(prev), next(next), hasValue(true), value(value) {
            prev->next = this;
            next->prev = this;
        }

        // Destruct a node. This removes the node from the list and de-allocates
        // the value held by this node.
        ~Node() {
            prev->next = next;
            next->prev = prev;
            if (hasValue) {
                value.~T();
            }
        }

        // The previous node in the list.
        Node* prev;
        // The next node in the list.
        Node* next;

        // Since we use a sentinel node approach, the head node does not contain
        // a value. Therefore we use a union so that this Node can optionally
        // contain a value. The other approach would be to dynamically allocate
        // each element, however this lead to potentially more memory
        // consumption (especially if T is small) and more pointer-chasing.
        // Indicate if this Node holds a value. If true, `value` contains a
        // valid, constructed object of type T, otherwise `value` is undefined.
        bool hasValue;
        union {
            T value;
        };
    };

    // The head of the list. This acts as a sentinel node that does not contain
    // a value. The sentinel node approach offers a significant advantage
    // compared to the approach saving a pointer to the first Node: there is no
    // special case when inserting the first/last element of the list.
    Node m_head;
};
