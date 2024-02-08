#include "./vectortests.hpp"
#include <datastruct/vector.hpp>
#include <selftests/macros.hpp>

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

namespace DataStruct {
// Check that a default Vector<Obj> does not allocate any array and does not
// call any contructor. Its size / capacity is expected to be zero at this
// point.
SelfTests::TestResult vectorDefaultConstructionTest() {
    counter.reset();
    Vector<Obj> vec;
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    TEST_ASSERT(vec.size() == 0);
    TEST_ASSERT(vec.empty());
    TEST_ASSERT(vec.capacity() == 0);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorConstructorSizeDefaultValueTest() {
    counter.reset();
    Vector<Obj> vec(16);
    TEST_ASSERT(vec.size() == 16);
    TEST_ASSERT(!vec.empty());
    TEST_ASSERT(vec.capacity() == 16);

    TEST_ASSERT(counter.defaultConstructor == 16);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorConstructorSizeWithValueTest() {
    Obj const obj;
    counter.reset();
    Vector<Obj> vec(32, obj);
    TEST_ASSERT(vec.size() == 32);
    TEST_ASSERT(!vec.empty());
    TEST_ASSERT(vec.capacity() == 32);

    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 32);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorDestructorTest() {
    {
        Vector<Obj> vec(16);
        counter.reset();
    }
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 16);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorAccessorTest() {
    u64 const vecSize(16);
    Obj objs[vecSize];

    for (u64 i(0); i < vecSize; ++i) {
        objs[i] = Obj(i);
    }

    Vector<Obj> vec(vecSize);
    // Check that all values are default.
    for (u64 i(0); i < vec.size(); ++i) {
        TEST_ASSERT(!vec[i].value);
    }

    // Set all values of the vector to their counter part in objs[].
    counter.reset();
    for (u64 i(0); i < vec.size(); ++i) {
        vec[i] = objs[i];
    }
    // Check that each assigment lead to calling the assignment operator.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == vec.size());
    TEST_ASSERT(counter.destructor == 0);

    // Now read each element of the vector through a const-ref to the vector.
    // Check that their value member corresponds to their objs[] counter part.
    Vector<Obj> const& constRef(vec);
    for (u64 i(0); i < constRef.size(); ++i) {
        TEST_ASSERT(constRef[i].value == objs[i].value);
    }
    // Reading through the const-ref should not have called any constructor,
    // assigment operator or destructor.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == vec.size());
    TEST_ASSERT(counter.destructor == 0);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorClearTest() {
    u64 const vecSize(32);
    {
        Vector<Obj> vec(vecSize);
        counter.reset();
        vec.clear();
        TEST_ASSERT(counter.defaultConstructor == 0);
        TEST_ASSERT(counter.userConstructor == 0);
        TEST_ASSERT(counter.copyConstructor == 0);
        TEST_ASSERT(counter.moveConstructor == 0);
        TEST_ASSERT(counter.assignment == 0);
        TEST_ASSERT(counter.destructor == vecSize);
        TEST_ASSERT(!vec.size());
        TEST_ASSERT(vec.empty());
    }
    // Double check that destructors are not called twice.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == vecSize);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorPushBackTest() {
    Vector<Obj> vec;
    vec.pushBack(Obj(0));

    u64 const numElems(1 + vec.capacity() - vec.size());
    Obj objs[numElems];
    for (u64 i(0); i < numElems; ++i) {
        objs[i] = Obj(i);
    }

    counter.reset();
    for (u64 i(1); vec.size() < vec.capacity(); ++i) {
        ASSERT(i < numElems);
        vec.pushBack(objs[i]);
        TEST_ASSERT(vec[i].value == objs[i].value);
    }
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == vec.capacity() - 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    // Next pushBack increases the capacity. This copies all existing elements
    // to the new array, using copy constructor, then destruct all those
    // elements. Finally it should construct the element to insert using the
    // copy constructor.
    u64 const prevSize(vec.size());
    ASSERT(prevSize == vec.capacity());
    counter.reset();
    vec.pushBack(objs[numElems - 1]);
    TEST_ASSERT(vec.size() == prevSize + 1);
    TEST_ASSERT(vec.capacity() == prevSize * 2);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == prevSize + 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == prevSize);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorPopBackTest() {
    u64 const numElems(1300);
    Vector<Obj> vec;
    for (u64 i(0); i < numElems; ++i) {
        vec.pushBack(Obj(i));
    }
    TEST_ASSERT(vec.size() == numElems);
    counter.reset();
    for (u64 i(0); i < numElems; ++i) {
        vec.popBack();
        TEST_ASSERT(vec.size() == numElems - 1 - i);
        TEST_ASSERT(counter.defaultConstructor == 0);
        TEST_ASSERT(counter.userConstructor == 0);
        TEST_ASSERT(counter.copyConstructor == 0);
        TEST_ASSERT(counter.moveConstructor == 0);
        TEST_ASSERT(counter.assignment == 0);
        TEST_ASSERT(counter.destructor == i + 1);
    }
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorInsertFrontTest() {
    Vector<Obj> vec;
    Obj const firstElem(0);
    // When inserting into an empty vector, the object is constructed in-place
    // an no element needs to be shifted, hence a single call to the copy
    // constructor.
    counter.reset();
    vec.insert(0, firstElem);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    u64 const numElems(vec.capacity() - vec.size());

    // Insert the rest of the elements at index 0.
    for (u64 i(1); i <= numElems; ++i) {
        Obj const obj(i);
        u64 const prevSize(vec.size());
        counter.reset();
        vec.insert(0, obj);
        // Now each insertion needs to:
        //  1. Shift all existing elements to the right.
        //  2. Insert the new element at index 0.
        // Step #1 requires copy-constructing the last element and assigning all
        // others.
        // Step #2 requires assigning the element at index 0 to the new element.
        // Therefore, the insertion calls:
        //  - copy constructor x1
        //  - assignment operator xVec.size() (size before the insert).
        TEST_ASSERT(vec.size() == prevSize + 1);
        TEST_ASSERT(counter.defaultConstructor == 0);
        TEST_ASSERT(counter.userConstructor == 0);
        TEST_ASSERT(counter.copyConstructor == 1);
        TEST_ASSERT(counter.moveConstructor == 0);
        TEST_ASSERT(counter.assignment == prevSize);
        TEST_ASSERT(counter.destructor == 0);
    }

    // Inserting one more element leads to a capacity increase.
    Obj const last(numElems+1);
    u64 const prevSize(vec.size());
    u64 const prevCap(vec.capacity());
    counter.reset();
    vec.insert(0, last);
    TEST_ASSERT(vec.size() == prevSize + 1);
    TEST_ASSERT(vec.capacity() == prevCap * 2);
    // The capacity increase led to prevSize calls to the copy constructor +
    // prevSize call to the destructor. Then the insert, as above, called the
    // copy constructor once and prevSize calls to the assignment operator.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == prevSize + 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == prevSize);
    TEST_ASSERT(counter.destructor == prevSize);

    // Sanity check: the values are in decreasing order.
    for (u64 i(0); i < vec.size(); ++i) {
        TEST_ASSERT(vec[i].value == vec.size() - i - 1);
    }
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorInsertMiddleTest() {
    // We are not necessarily testing inserting in the middle here, just testing
    // inserting at an index that is not an extremity of the vector.
    Vector<Obj> vec;

    // Fill the vector until we have one spot available before a resize.
    for (u64 i(0); !vec.size() || vec.size() < vec.capacity() - 1; ++i) {
        vec.pushBack(Obj(i));
    }

    // At this point the vector contains: 0, 1, 2, 3, 4, ... Insert 100 at index
    // 4.
    Obj const newElem(100);
    counter.reset();
    u64 prevSize(vec.size());
    u64 prevCap(vec.capacity());
    vec.insert(4, newElem);
    // The insert should have shifted prevSize-4 elements, all excepted one was
    // shifted with an assignement operator, the last element was
    // copy-constructed. The new element was inserted by using the assignment
    // operator.
    TEST_ASSERT(vec.size() == prevSize + 1);
    TEST_ASSERT(vec.capacity() == prevCap);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == (prevSize - 4) - 1 + 1);
    TEST_ASSERT(counter.destructor == 0);

    // The next insert is going to trigger a re-allocation.
    Obj const newElem2(200);
    counter.reset();
    prevSize = vec.size();
    prevCap = vec.capacity();
    vec.insert(5, newElem2);
    TEST_ASSERT(vec.size() == prevSize + 1);
    TEST_ASSERT(vec.capacity() == prevCap * 2);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == prevSize + 1);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == (prevSize - 5) - 1 + 1);
    TEST_ASSERT(counter.destructor == prevSize);

    // Check the content of the vector:
    for (u64 i(0); i < vec.size(); ++i) {
        // Expected sequence:
        //  Index |  0  1  2  3  4   5    6  7  8 ..
        //  Value |  0  1  2  3  100 200  4  5  6 ...
        u64 const expValue(i == 4 ? 100 : (i == 5 ? 200 : (i < 4 ? i : (i-2))));
        TEST_ASSERT(vec[i].value == expValue);
    }
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorEraseTest() {
    u64 const numElems(128);
    Vector<Obj> vec;
    for (u64 i(0); i < numElems; ++i) {
        vec.pushBack(Obj(i));
    }
    u64 const cap(vec.capacity());

    // Case #1: Erase the first element.
    counter.reset();
    u64 prevSize(vec.size());
    vec.erase(0);
    TEST_ASSERT(vec.size() == prevSize - 1);
    // Erasing does not change the capacity.
    TEST_ASSERT(vec.capacity() == cap);
    // Erasing means shifting all elements, starting at `index + 1`, to the left
    // using the assigment operator. The last element is then destroyed.
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == prevSize - 1);
    TEST_ASSERT(counter.destructor == 1);
    for (u64 i(0); i < vec.size(); ++i) {
        // Expected sequence:
        //  Index |  0 1 2 3 4 5 6 ...
        //  Value |  1 2 3 4 5 6 7 ...
        u64 const expValue(i + 1);
        TEST_ASSERT(vec[i].value == expValue);
    }

    // Case #2: Erase in the "middle" of the vector.
    counter.reset();
    prevSize = vec.size();
    vec.erase(4);
    TEST_ASSERT(vec.size() == prevSize - 1);
    TEST_ASSERT(vec.capacity() == cap);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == prevSize - 4 - 1);
    TEST_ASSERT(counter.destructor == 1);
    for (u64 i(0); i < vec.size(); ++i) {
        // Expected sequence:
        //  Index |  0 1 2 3 4 5 6 ...
        //  Value |  1 2 3 4 6 7 8 ...
        u64 const expValue(i + ((i < 4) ? 1 : 2));
        TEST_ASSERT(vec[i].value == expValue);
    }

    // Case #3: Erase at the end of the vector.
    counter.reset();
    prevSize = vec.size();
    vec.erase(prevSize - 1);
    TEST_ASSERT(vec.size() == prevSize - 1);
    TEST_ASSERT(vec.capacity() == cap);
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    // Only the last element was destroyed, no shifting occured.
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 1);

    for (u64 i(0); i < vec.size(); ++i) {
        // Expected sequence:
        //  Index |  0 1 2 3 4 5 6 ... 124 125 126
        //  Value |  1 2 3 4 6 7 8 ... 126 127 128
        u64 const expValue(i + ((i < 4) ? 1 : 2));
        TEST_ASSERT(vec[i].value == expValue);
    }

    // Case #4: Erase the only element from the vector.
    vec.clear();
    vec.pushBack(Obj(1000));
    counter.reset();
    vec.erase(0);
    TEST_ASSERT(vec.empty());
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == 0);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 1);
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorIteratorTest() {
    u64 const numElems(128);
    Vector<Obj> vec;
    for (u64 i(0); i < numElems; ++i) {
        vec.pushBack(Obj(i));
    }

    // Case #1: Iterate with a for each loop.
    u64 idx(0);
    for (Obj const& elem : vec) {
        TEST_ASSERT(elem == vec[idx]);
        idx++;
    }

    // Case #2: Modify elements with a for each loop.
    for (Obj& elem : vec) {
        elem.value *= 2;
    }
    for (u64 i(0); i < vec.size(); ++i) {
        TEST_ASSERT(vec[i].value == i*2);
    }

    // Case #3: Iterator for a const-ref of a vector.
    Vector<Obj> const& constRef(vec);
    idx = 0;
    for (Obj const& elem : constRef) {
        TEST_ASSERT(elem == vec[idx]);
        idx++;
    }

    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorCopyTest() {
    u64 const numElems(128);
    Vector<Obj> vec1;
    for (u64 i(0); i < numElems; ++i) {
        vec1.pushBack(Obj(i));
    }

    counter.reset();
    Vector<Obj> vec2(vec1);
    TEST_ASSERT(vec2.size() == vec1.size());

    // "No destructor in sight, only values enjoying the copy-constructor".
    TEST_ASSERT(counter.defaultConstructor == 0);
    TEST_ASSERT(counter.userConstructor == 0);
    TEST_ASSERT(counter.copyConstructor == numElems);
    TEST_ASSERT(counter.moveConstructor == 0);
    TEST_ASSERT(counter.assignment == 0);
    TEST_ASSERT(counter.destructor == 0);

    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(vec1[i] == vec2[i]);
    }

    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorAssignTest() {
    u64 const numElems(128);
    Vector<Obj> vec1;
    Vector<Obj> vec2;
    for (u64 i(0); i < numElems; ++i) {
        vec1.pushBack(Obj(i));
        vec2.pushBack(Obj(numElems - i));
    }

    Vector<Obj> vec3;
    TEST_ASSERT(vec3.size() == 0);

    vec3 = vec1;
    TEST_ASSERT(vec3.size() == vec1.size());
    for (u64 i(0); i < vec3.size(); ++i) {
        TEST_ASSERT(vec3[i] == vec1[i]);
    }
    vec3 = vec2;
    TEST_ASSERT(vec3.size() == vec2.size());
    for (u64 i(0); i < vec3.size(); ++i) {
        TEST_ASSERT(vec3[i] == vec2[i]);
    }
    return SelfTests::TestResult::Success;
}

SelfTests::TestResult vectorComparisonTest() {
    u64 const numElems(128);

    // Case #1: Copy construction.
    Vector<Obj> vec1;
    for (u64 i(0); i < numElems; ++i) {
        vec1.pushBack(Obj(i));
    }
    Vector<Obj> vec2(vec1);
    TEST_ASSERT(vec1 == vec2);
    TEST_ASSERT(vec2 == vec1);

    // Case #2: Assignment.
    Vector<Obj> vec3;
    TEST_ASSERT(vec1 != vec3);
    TEST_ASSERT(vec3 != vec1);
    vec3 = vec1;
    TEST_ASSERT(vec1 == vec3);
    TEST_ASSERT(vec3 == vec1);

    // Case #3: Different values.
    vec3[0].value = 100;
    TEST_ASSERT(vec1 != vec3);
    TEST_ASSERT(vec3 != vec1);

    // Case #4: Different sizes.
    vec2.insert(0, 100);
    TEST_ASSERT(vec1 != vec2);
    TEST_ASSERT(vec2 != vec1);
    vec2.erase(0);
    TEST_ASSERT(vec1 == vec2);
    TEST_ASSERT(vec2 == vec1);
    return SelfTests::TestResult::Success;
}
}
