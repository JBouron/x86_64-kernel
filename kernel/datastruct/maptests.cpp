#include <datastruct/map.hpp>
#include <selftests/macros.hpp>
#include "./counterobj.hpp"


namespace DataStruct {

// Custom, non-trivial, key type for map tests.
struct Key {
    Key(u64 const v) : value(v) {}
    bool operator==(Key const& o) const = default;
    u64 value;
};

SelfTests::TestResult mapDefaultConstructionTest() {
    CounterObj::counter.reset();
    Map<Key, CounterObj> map;

    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == 0);

    TEST_ASSERT(!map.size());
    TEST_ASSERT(map.empty());
    return SelfTests::TestResult::Success;
}

// Insert elements in a map, look them up in the map and destroy the map all
// while re-hashing is disabled. The goal here is to make sure the
// implementation handles a high load factor.
SelfTests::TestResult mapInsertionLookupAndDestructorTestNoRehash() {
    u64 const numElems(1024);
    {
        // The map is set to use a number of buckets that is much smaller than
        // the number of elements we are about to insert in order to simulate a
        // high load factor.
        Map<Key, CounterObj> map(8, false);
        TEST_ASSERT(!map.size());
        TEST_ASSERT(map.empty());

        CounterObj::counter.reset();

        // Insert.
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(!map.contains(Key(i)));
            map[Key(i)] = CounterObj(i);
            TEST_ASSERT(map.size() == i + 1);
            TEST_ASSERT(!map.empty());
        }

        // In each loop:
        //  1. A temporary CounterObj is created.
        //  2. The key is looked up in the map but is not contained, hence the
        //  map creates a default CounterObj for this key. This default value is
        //  created, copied (copy-constructor) into the bucket and destroyed.
        //  See FIXME comment in Map<T>::operator[].
        //  3. The value associated to the key is assigned the value of the
        //  temporary
        //  4. The temporary is destroyed.
        TEST_ASSERT(CounterObj::counter.defaultConstructor == numElems);
        TEST_ASSERT(CounterObj::counter.userConstructor == numElems);
        TEST_ASSERT(CounterObj::counter.copyConstructor == numElems);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == numElems);
        TEST_ASSERT(CounterObj::counter.destructor == 2 * numElems);

        Map<Key, CounterObj> const& mapRef(map);

        CounterObj::counter.reset();

        // Lookup.
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(mapRef.contains(Key(i)));
            TEST_ASSERT(mapRef[Key(i)].value == i);
        }

        // Looking up the values in the map should not have created any new
        // copies.
        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == 0);
    }
    // The Map destructor is expected to call the destructor on all values it
    // contains.
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor ==  numElems);

    return SelfTests::TestResult::Success;
}

// The the rehash behaviour of Map<T>.
SelfTests::TestResult mapRehashTest() {
    // Number of elements inserted so far.
    u64 numElems(0);
    {
        // Start with a small number of buckets.
        Map<Key, CounterObj> map(8);
        TEST_ASSERT(!map.size());
        TEST_ASSERT(map.empty());

        // We are going to insert enough elements until we reach the desired
        // number of rehashes.
        u64 const numRehash(10);

        for (u64 _(0); _ < numRehash; ++_) {
            // Insert until we reach the rehash condition.
            while (map.size() != map.numBuckets()) {
                TEST_ASSERT(!map.contains(Key(numElems)));
                map[Key(numElems)] = CounterObj(numElems);
                TEST_ASSERT(map.size() == numElems + 1);
                TEST_ASSERT(!map.empty());
                ++numElems;
            }

            // The next insert is going to trigger a rehash.
            CounterObj::counter.reset();
            u64 const sizeBefore(map.size());
            {
                map[Key(numElems)] = CounterObj(numElems);
                ++numElems;
            }
            TEST_ASSERT(map.size() == sizeBefore + 1);

            // Before inserting a new value in the map, the implementation
            // rehashes the entire map. With N = map.size(), we expect:
            //  1. N copy constructor calls, due to the rehash re-creating the
            //  new bucket.
            //  2. N destructor calls, due to the rehash destroying the old
            //  buckets.
            // Then when inserting the new value:
            //  1. A temporary CounterObj is created.
            //  2. The key is looked up in the map but is not contained, hence
            //  the map creates a default CounterObj for this key. This default
            //  value is created, copied (copy-constructor) into the bucket and
            //  destroyed. See FIXME comment in Map<T>::operator[].
            //  3. The value associated to the key is assigned the value of the
            //  temporary
            //  4. The temporary is destroyed.
            TEST_ASSERT(CounterObj::counter.defaultConstructor == 1);
            TEST_ASSERT(CounterObj::counter.userConstructor == 1);
            TEST_ASSERT(CounterObj::counter.copyConstructor == sizeBefore + 1);
            TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
            TEST_ASSERT(CounterObj::counter.assignment == 1);
            TEST_ASSERT(CounterObj::counter.destructor == sizeBefore + 2);
        }
        CounterObj::counter.reset();
    }
    // The Map destructor is expected to call the destructor on all values it
    // contains.
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor ==  numElems);

    return SelfTests::TestResult::Success;
}

// Test the rehash behaviour of Map<T>.
SelfTests::TestResult mapEraseTest() {
    u64 const numElems(1024);
    // Artificially increase the load factor by disabling rehashing.
    Map<Key, CounterObj> map(8, false);
    TEST_ASSERT(!map.size());
    TEST_ASSERT(map.empty());

    // Insert.
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(!map.contains(Key(i)));
        map[Key(i)] = CounterObj(i);
        TEST_ASSERT(map.size() == i + 1);
        TEST_ASSERT(!map.empty());
    }

    CounterObj::counter.reset();

    // Remove elements one-by-one.
    for (u64 i(0); i < numElems; ++i) {
        Key const key(i);
        TEST_ASSERT(map.contains(key));
        map.erase(key);
        TEST_ASSERT(!map.contains(key));
        TEST_ASSERT(map.size() == numElems - i - 1);

        // Also erase keys that were not inserted in the map. This should be a
        // no-op.
        Key const bogusKey(numElems + i);
        TEST_ASSERT(!map.contains(bogusKey));
        map.erase(bogusKey);
        TEST_ASSERT(map.size() == numElems - i - 1);

        // Each removal should destroy the value. No new copy is created.
        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == i + 1);
    }
    TEST_ASSERT(map.empty());
    return SelfTests::TestResult::Success;
}

// Test clearing a map out of all of its elements.
SelfTests::TestResult mapClearTest() {
    u64 const numElems(1024);
    Map<Key, CounterObj> map;
    TEST_ASSERT(!map.size());
    TEST_ASSERT(map.empty());

    // Insert.
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(!map.contains(Key(i)));
        map[Key(i)] = CounterObj(i);
        TEST_ASSERT(map.contains(Key(i)));
        TEST_ASSERT(map.size() == i + 1);
        TEST_ASSERT(!map.empty());
    }

    // Clear the map, this should call the destructor on all inserted elements.
    CounterObj::counter.reset();
    map.clear();
    TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
    TEST_ASSERT(CounterObj::counter.userConstructor == 0);
    TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
    TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
    TEST_ASSERT(CounterObj::counter.assignment == 0);
    TEST_ASSERT(CounterObj::counter.destructor == numElems);

    TEST_ASSERT(!map.size());
    TEST_ASSERT(map.empty());

    // Check that the map no longer contain the elements.
    for (u64 i(0); i < numElems; ++i) {
        TEST_ASSERT(!map.contains(Key(i)));
    }
    return SelfTests::TestResult::Success;
}

// Check calling some member functions on a default-constructed map. The point
// of this test is to check that we are not triggering any asserts doing so.
SelfTests::TestResult mapOpOnDefaultMap() {
    Map<Key, CounterObj> map;
    TEST_ASSERT(!map.size());
    TEST_ASSERT(map.empty());
    TEST_ASSERT(!map.contains(Key(1337)));
    map.erase(Key(0xdead));
    map.clear();
    TEST_ASSERT(!map.numBuckets());
    return SelfTests::TestResult::Success;
}

// Special key type for which the hash always return the same value, no matter
// the value of the key itself. This is used to artificially increase the amount
// of hash collision when inserting/reading/deleting from a map.
struct HighCollisionKey {
    HighCollisionKey(u64 const v) : value(v) {}
    bool operator==(HighCollisionKey const& o) const = default;
    u64 value;
};

// Comprehensive test where all Keys share the same hash.
SelfTests::TestResult mapHighHashCollisionTest() {
    u64 const numElems(1024);
    {
        // The map is set to use a number of buckets that is much smaller than
        // the number of elements we are about to insert in order to simulate a
        // high load factor.
        Map<HighCollisionKey, CounterObj> map;
        TEST_ASSERT(!map.size());
        TEST_ASSERT(map.empty());

        // Insert.
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(!map.contains(HighCollisionKey(i)));
            map[HighCollisionKey(i)] = CounterObj(i);
            TEST_ASSERT(map.size() == i + 1);
            TEST_ASSERT(!map.empty());
        }

        Map<HighCollisionKey, CounterObj> const& mapRef(map);

        // Lookup.
        for (u64 i(0); i < numElems; ++i) {
            TEST_ASSERT(mapRef.contains(HighCollisionKey(i)));
            TEST_ASSERT(mapRef[HighCollisionKey(i)].value == i);
        }

        CounterObj::counter.reset();

        // Remove all even keys.
        for (u64 i(0); i < numElems; i += 2) {
            HighCollisionKey const key(i);
            TEST_ASSERT(map.contains(key));
            map.erase(key);
        }
        TEST_ASSERT(map.size() == numElems / 2);
        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == numElems / 2);

        for (u64 i(0); i < numElems; ++i) {
            HighCollisionKey const key(i);
            if (i % 2 == 0) {
                TEST_ASSERT(!map.contains(key));
            } else {
                TEST_ASSERT(map.contains(key));
            }
        }

        // Remove all odd keys.
        for (u64 i(1); i < numElems; i += 2) {
            HighCollisionKey const key(i);
            TEST_ASSERT(map.contains(key));
            map.erase(key);
        }
        TEST_ASSERT(!map.size());
        TEST_ASSERT(map.empty());
        TEST_ASSERT(CounterObj::counter.defaultConstructor == 0);
        TEST_ASSERT(CounterObj::counter.userConstructor == 0);
        TEST_ASSERT(CounterObj::counter.copyConstructor == 0);
        TEST_ASSERT(CounterObj::counter.moveConstructor == 0);
        TEST_ASSERT(CounterObj::counter.assignment == 0);
        TEST_ASSERT(CounterObj::counter.destructor == numElems);
    }
    return SelfTests::TestResult::Success;
}
}

// Specialization of the hash<T>() for the Key type used in the map tests.
template<>
u64 hash<DataStruct::Key>(DataStruct::Key const& value) {
    return value.value;
}

// Specialization of the hash<T>() for the HighCollisionKey type used in the map
// tests. Always return the same value.
template<>
u64 hash<DataStruct::HighCollisionKey>(DataStruct::HighCollisionKey const&) {
    return 0xdeadbeefcafebabe;
}
