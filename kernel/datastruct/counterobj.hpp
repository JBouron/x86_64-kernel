#pragma once

namespace DataStruct {

// Object type counting the number of time constructors, destructor and
// assignment operators are invoked. This is a useful type to assert that data
// structures are constructing and destroying objects as expected.
// This type also contains a u64 member value so that it is not a trivial empty
// type.
class CounterObj {
public:
    // Hold the number of times the various ctors, dtor and operators are
    // called.
    struct Counter {
        u64 defaultConstructor;
        u64 userConstructor;
        u64 copyConstructor;
        u64 moveConstructor;
        u64 assignment;
        u64 destructor;

        // Reset all counters.
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

    CounterObj() : value(0) {
        counter.defaultConstructor++;
    }

    CounterObj(u64 const val) : value(val) {
        counter.userConstructor++;
    }

    CounterObj(CounterObj const& other) : value(other.value) {
        counter.copyConstructor++;
    }

    CounterObj(CounterObj && other) : value(other.value) {
        counter.moveConstructor++;
    }

    void operator=(CounterObj const& other) {
        value = other.value;
        counter.assignment++;
    }

    ~CounterObj() {
        counter.destructor++;
    }

    bool operator==(CounterObj const& other) const = default;

    u64 value;
};


};
