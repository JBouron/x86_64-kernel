#include <datastruct/list.hpp>
#include <selftests/macros.hpp>

namespace DataStruct {

// FIXME: Counter and Obj should be declared in a header that can be included
// in the various tests that needs them.
struct Counter {
    u64 defaultConstructor;
    u64 userConstructor;
    u64 copyConstructor;
    u64 moveConstructor;

    u64 assignment;

    u64 destructor;

    void reset() {
        defaultConstructor = 0;
        userConstructor = 0;
        copyConstructor = 0;
        moveConstructor = 0;
        assignment = 0;
        destructor = 0;
    }
};

static Counter counter;

class Obj {
public:
    Obj() : value(0) {
        counter.defaultConstructor++;
    }

    Obj(u64 const val) : value(val) {
        counter.userConstructor++;
    }

    Obj(Obj const& other) : value(other.value) {
        counter.copyConstructor++;
    }

    Obj(Obj && other) : value(other.value) {
        counter.moveConstructor++;
    }

    void operator=(Obj const& other) {
        value = other.value;
        counter.assignment++;
    }

    ~Obj() {
        counter.destructor++;
    }

    bool operator==(Obj const& other) const = default;

    u64 value;
};

// Check that the default constructor of List<T> creates an empty list.
SelfTests::TestResult listConstructionTest() {
    counter.reset();

    List<Obj> list;

    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    TEST_ASSERT(list.size() == 0);
    TEST_ASSERT(list.empty());

    return SelfTests::TestResult::Success;
}

// Check the pushBack(), pushFront(), back(), front() and iterator of List<T>.
SelfTests::TestResult listPushAndIterationTest() {
    List<Obj> list;

    counter.reset();

    u64 expectedNumUserCtorCalls(0);
    u64 expectedNumCopyCtorCalls(0);
    u64 numPushes(0);

    for (i64 i(127); i >= 0; --i) {
        Obj const value(i);
        expectedNumUserCtorCalls++;
        TEST_ASSERT(counter.userConstructor == expectedNumUserCtorCalls);
        TEST_ASSERT(counter.copyConstructor == expectedNumCopyCtorCalls);

        list.pushFront(value);

        expectedNumCopyCtorCalls++;
        TEST_ASSERT(counter.userConstructor == expectedNumUserCtorCalls);
        TEST_ASSERT(counter.copyConstructor == expectedNumCopyCtorCalls);

        // Sanity check: front() should return the value we just pushed.
        TEST_ASSERT(list.front() == value);

        numPushes++;
        TEST_ASSERT(list.size() == numPushes);
        TEST_ASSERT(!list.empty());
    }

    for (u64 i(128); i < 256; ++i) {
        Obj const value(i);
        expectedNumUserCtorCalls++;
        TEST_ASSERT(counter.userConstructor == expectedNumUserCtorCalls);
        TEST_ASSERT(counter.copyConstructor == expectedNumCopyCtorCalls);

        list.pushBack(value);

        expectedNumCopyCtorCalls++;
        TEST_ASSERT(counter.userConstructor == expectedNumUserCtorCalls);
        TEST_ASSERT(counter.copyConstructor == expectedNumCopyCtorCalls);

        // Sanity check: back() should return the value we just pushed.
        TEST_ASSERT(list.back() == value);

        numPushes++;
        TEST_ASSERT(list.size() == numPushes);
        TEST_ASSERT(!list.empty());
    }
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    // Due to the temporaries.
    TEST_ASSERT(counter.destructor == 256);

    u64 nextExpectedValue(0);
    for (Obj const& obj: list) {
        TEST_ASSERT(obj.value == nextExpectedValue);
        nextExpectedValue++;
    }
    // This checks that we indeed iterated over the entire list.
    TEST_ASSERT(nextExpectedValue == 256);

    return SelfTests::TestResult::Success;
}

// Check modifying elements of a List<T> either through front()/back(), or
// through an iterator.
SelfTests::TestResult listInplaceModificationTest() {
    List<Obj> list;

    u64 const numElems(1024);
    for (u64 i(0); i < numElems; ++i) {
        list.pushBack(Obj(i));
    }
    TEST_ASSERT(list.size() == numElems);
    TEST_ASSERT(!list.empty());

    // Modify through front().
    TEST_ASSERT(list.front().value == 0);
    list.front().value = 1337;
    TEST_ASSERT(list.front().value == 1337);
    list.front().value = 0;

    // Modify through back().
    TEST_ASSERT(list.back().value == numElems - 1);
    list.back().value = 8086;
    TEST_ASSERT(list.back().value == 8086);
    list.back().value = numElems - 1;

    // Modify through an iterator.
    for (Obj& obj : list) {
        obj.value *= 2;
    }

    // Iterate through the list, counting the number of elements and checking
    // that their value was doubled.
    u64 index(0);
    for (Obj const& obj : list) {
        TEST_ASSERT(obj.value == index * 2);
        index ++;
    }
    TEST_ASSERT(index == numElems);

    // Double each element again, this time assigning the elements.
    counter.reset();
    for (Obj& obj : list) {
        obj = Obj(obj.value * 2);
    }
    TEST_ASSERT(counter.userConstructor == numElems);
    TEST_ASSERT(counter.assignment == numElems);
    index = 0;
    for (Obj const& obj : list) {
        TEST_ASSERT(obj.value == index * 4);
        index ++;
    }
    TEST_ASSERT(index == numElems);

    return SelfTests::TestResult::Success;
}

// Check removing elements from a List<T> using the iterator erase() method.
SelfTests::TestResult listEraseTest() {
    List<Obj> list;

    u64 const numElems(1024);
    // Required by this test.
    ASSERT(numElems % 2 == 0);
    for (u64 i(0); i < numElems; ++i) {
        list.pushBack(Obj(i));
    }

    counter.reset();

    // Iterate through the whole list and remove all even elements.
    {
        List<Obj>::Iter ite(list.begin());
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(ite != list.end());
            Obj const& elem(*ite);
            TEST_ASSERT(elem.value == i);
            if (elem.value % 2 == 0) {
                ite.erase();
            } else {
                ++ite;
            }
        }
        TEST_ASSERT(ite == list.end());
    }
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == numElems / 2);

    // At this point only half of the elements are left. Check that they're all
    // odd.
    TEST_ASSERT(list.size() == numElems / 2);
    {
        List<Obj>::Iter ite(list.begin());
        for (u64 i(0); i < numElems / 2; ++i) {
            TEST_ASSERT(ite != list.end());
            TEST_ASSERT((*ite).value == i * 2 + 1);
            ++ite;
        }
        TEST_ASSERT(ite == list.end());
    }

    // Iterate through the rest of the list and remove the remaining elements.
    {
        List<Obj>::Iter ite(list.begin());
        for (u64 i(0); i < numElems / 2; ++i) {
            TEST_ASSERT(ite != list.end());
            ite.erase();
        }
        TEST_ASSERT(ite == list.end());
    }
    TEST_ASSERT(!list.size());
    TEST_ASSERT(list.empty());
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == numElems);

    return SelfTests::TestResult::Success;
}

// Check popping elements out of a list using popFront() and popBack().
SelfTests::TestResult listPopTest() {
    List<Obj> list;

    // Case 1: Empty the list using popFront().
    u64 const numElems(1024);
    for (u64 i(0); i < numElems; ++i) {
        list.pushBack(Obj(i));
    }

    counter.reset();
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(list.size() == numElems - i);
        TEST_ASSERT(list.popFront().value == i);
        TEST_ASSERT(counter.defaultConstructor == 0);
        TEST_ASSERT(counter.userConstructor == 0);
        TEST_ASSERT(counter.copyConstructor == i+1);
        TEST_ASSERT(counter.moveConstructor == 0);
        TEST_ASSERT(counter.assignment == 0);
        // Twice the destruction because of the copy returned by popFront().
        TEST_ASSERT(counter.destructor == 2 * (i+1));
    }
    TEST_ASSERT(list.empty());

    // Case 2: Empty the list using popBack().
    for (u64 i(0); i < numElems; ++i) {
        list.pushBack(Obj(i));
    }

    counter.reset();
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(list.size() == numElems - i);
        TEST_ASSERT(list.popBack().value == numElems - 1 - i);
        TEST_ASSERT(counter.defaultConstructor == 0);
        TEST_ASSERT(counter.userConstructor == 0);
        TEST_ASSERT(counter.copyConstructor == i+1);
        TEST_ASSERT(counter.moveConstructor == 0);
        TEST_ASSERT(counter.assignment == 0);
        // Twice the destruction because of the copy returned by popFront().
        TEST_ASSERT(counter.destructor == 2 * (i+1));
    }
    TEST_ASSERT(list.empty());

    return SelfTests::TestResult::Success;
}

// Check that elements are deleted/destroyed upon destruction of the List<T>.
SelfTests::TestResult listDestructorTest() {
    u64 const numElems(1024);
    {
        List<Obj> list;
        for (u64 i(0); i < numElems; ++i) {
            list.pushBack(Obj(i));
        }
        counter.reset();
    }
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == numElems);
    return SelfTests::TestResult::Success;
}

// Check the clear() method.
SelfTests::TestResult listClearTest() {
    List<Obj> list;
    u64 const numElems(1024);
    for (u64 i(0); i < numElems; ++i) {
        list.pushBack(Obj(i));
    }
    counter.reset();
    list.clear();
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == numElems);
    return SelfTests::TestResult::Success;
}

// Test the copy constructor.
SelfTests::TestResult listCopyConstructorTest() {
    u64 const numElems(1024);
    List<Obj> list1;
    for (u64 i(0); i < numElems; ++i) {
        list1.pushBack(Obj(i));
    }

    counter.reset();
    List<Obj> list2(list1);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == numElems);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    TEST_ASSERT(list1.size() == list2.size());

    // Check that all elements were copied.
    {
        List<Obj>::Iter iter2(list2.begin());
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(iter2 != list2.end());
            TEST_ASSERT((*iter2).value == i);
            ++iter2;
        }
        TEST_ASSERT(iter2 == list2.end());
    }

    // Check that a deep copy was performed by modifying the elements of list2.
    for (Obj& elem : list2) {
        elem.value *= 2;
    }

    {
        List<Obj>::Iter iter1(list1.begin());
        List<Obj>::Iter iter2(list2.begin());
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(iter1 != list1.end());
            TEST_ASSERT(iter2 != list2.end());
            TEST_ASSERT((*iter2).value == 2 * (*iter1).value);
            ++iter1;
            ++iter2;
        }
        TEST_ASSERT(iter1 == list1.end());
        TEST_ASSERT(iter2 == list2.end());
    }

    // Clear list1, list2 should not change.
    list1.clear();
    TEST_ASSERT(list2.size() == numElems);

    return SelfTests::TestResult::Success;
}

// Test List<T>::operator==().
SelfTests::TestResult listComparisonTest() {
    u64 const numElems(1024);
    List<Obj> list1;
    List<Obj> list2;
    for (u64 i(0); i < numElems; ++i) {
        list1.pushBack(Obj(i));
        list2.pushBack(Obj(i));
    }

    counter.reset();

    TEST_ASSERT(list1 == list1);
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);
    TEST_ASSERT(list2 == list2);

    // Sanity check: comparison should not create useless copies.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    // Change one element at the front.
    list1.front().value = 1337;
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    list2.front().value = 1337;
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Change one element in the "middle".
    (*(++list1.begin())).value = 8086;
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    (*(++list2.begin())).value = 8086;
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Change one element at the back.
    list1.back().value = 0xdeadbeef;
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    list2.back().value = 0xdeadbeef;
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Remove an element from the front.
    list1.popFront();
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    list2.popFront();
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Remove an element in the "middle".
    (++list1.begin()).erase();
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    (++list2.begin()).erase();
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Remove an element from the back.
    list1.popBack();
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    list2.popBack();
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    // Remove all elements.
    list1.clear();
    TEST_ASSERT(list1 != list2);
    TEST_ASSERT(list2 != list1);
    list2.clear();
    TEST_ASSERT(list1 == list2);
    TEST_ASSERT(list2 == list1);

    return SelfTests::TestResult::Success;
}

// Test List<T>::operator=(List<T> const&).
SelfTests::TestResult listAssignmentTest() {
    u64 const numElems(1024);
    List<Obj> list1;
    List<Obj> list2;
    // Construct two lists, one is smaller than the other.
    for (u64 i(0); i < numElems; ++i) {
        list1.pushBack(Obj(i));
        if (i < numElems / 2) {
            list2.pushBack(Obj(2 * i));
        }
    }

    TEST_ASSERT(list1 != list2);

    counter.reset();

    // The assignment should empty the destination list before copying the
    // elements one by one using the copy-constructor. See comment in
    // operator=().
    list2 = list1;
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == numElems);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == numElems/2);

    TEST_ASSERT(list1 == list2);

    // Check that the assigment performed a deep copy of the elements.
    for (Obj& elem : list2) {
        elem.value *= 3;
    }
    TEST_ASSERT(list1 != list2);

    return SelfTests::TestResult::Success;
}
}
