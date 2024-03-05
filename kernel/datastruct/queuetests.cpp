// Tests for the Queue<T> data structure.
#include <datastruct/queue.hpp>
#include <selftests/macros.hpp>
#include "./counterobj.hpp"

namespace DataStruct {
// Check that constructing a queue does not allocate any value.
SelfTests::TestResult queueConstructionTest() {
    CounterObj::counter.reset();

    {
        Queue<CounterObj> q;
        TEST_ASSERT(!q.size());
        TEST_ASSERT(q.empty());
    }

    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == 0);
    return SelfTests::TestResult::Success;
}

// Basic enqueuing and dequeuing test.
SelfTests::TestResult queueEnqueueDequeueTest() {
    u64 const numElems(2048);
    Queue<CounterObj> q;

    // Enqueue values.
    for (u64 i(0); i < numElems; ++i) {
        CounterObj const value(i);

        CounterObj::counter.reset();
        q.enqueue(value);
        TEST_ASSERT(q.size() == i + 1);
        TEST_ASSERT(!q.empty());

        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 1);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == 0);
    }

    // Dequeue values.
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(!q.empty());
        TEST_ASSERT(q.size() == numElems - i);

        CounterObj::counter.reset();
        CounterObj const value(q.dequeue());
        TEST_ASSERT(value.value == i);

        // Dequeue creates a copy of the head of the queue and destroy the
        // original value from the queue.
        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 1);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == 1);
    }
    TEST_ASSERT(q.size() == 0);
    TEST_ASSERT(q.empty());

    return SelfTests::TestResult::Success;
}

// Check the front and back accessors of Queue<T>.
SelfTests::TestResult queueFrontBackTest() {
    Queue<CounterObj> q;

    q.enqueue(CounterObj(0xdead));
    q.enqueue(CounterObj(0xbeef));

    CounterObj::counter.reset();

    Queue<CounterObj> const& qRef(q);
    TEST_ASSERT(q.front().value == 0xdead);
    TEST_ASSERT(q.back().value == 0xbeef);
    TEST_ASSERT(qRef.front().value == 0xdead);
    TEST_ASSERT(qRef.back().value == 0xbeef);

    // Accessing the front and the back does not create any copy.
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == 0);

    q.front().value = 0xcafe;
    q.back().value = 0xd00d;

    TEST_ASSERT(q.front().value == 0xcafe);
    TEST_ASSERT(q.back().value == 0xd00d);
    TEST_ASSERT(qRef.front().value == 0xcafe);
    TEST_ASSERT(qRef.back().value == 0xd00d);

    // Modifying the front and back does not create new objects.
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == 0);

    TEST_ASSERT(q.dequeue().value == 0xcafe);
    TEST_ASSERT(q.dequeue().value == 0xd00d);

    return SelfTests::TestResult::Success;
}

// Check Queue<T>::~Queue() and Queue<T>::clear().
SelfTests::TestResult queueClearAndDestructorTest() {
    u64 const numElems(2048);
    // Case #1: Calling clear should destroy all elements.
    {
        Queue<CounterObj> q;

        for (u64 i(0); i < numElems; ++i) {
            q.enqueue(CounterObj(i));
        }
        TEST_ASSERT(q.size() == numElems);

        CounterObj::counter.reset();

        q.clear();
        TEST_ASSERT(!q.size());
        TEST_ASSERT(q.empty());

        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == numElems);
    }

    // Case #2: The destructor destroys all elements.
    {
        Queue<CounterObj> q;

        for (u64 i(0); i < numElems; ++i) {
            q.enqueue(CounterObj(i));
        }
        TEST_ASSERT(q.size() == numElems);

        CounterObj::counter.reset();
    }
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == numElems);
    return SelfTests::TestResult::Success;
}

}
