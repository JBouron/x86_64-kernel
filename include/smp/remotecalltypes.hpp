// Execute a function on a remote CPU.
#pragma once
#include <concurrency/atomic.hpp>

namespace Smp::RemoteCall {

// Represents a function call to be performed by a cpu. Each cpu maintains a
// queue of CallDescs* in their PerCpu::Data.
// A cpu executing a CallDesc simply calls the invoke() virtual method on the
// object. This invoke() method does the actual call to the function and takes
// care of eventual parameters and/or return values.
class CallDesc {
public:
    virtual ~CallDesc() = default;
    virtual void invoke() = 0;
};

// Implementation of CallDesc for a function taking no argument and returning no
// value. Because static functions and lambdas have different types (lambdas
// having implementation specific types that also depend on their capture list),
// this type is templated on the type of the function to be called, namely Func.
// By supporting calling lambdas with no params and no return value, we by
// extension support calling any kind of function with/without params and/or
// return value by simply wrapping them into a lambda. See the definition of
// invokeOn() to see this trick in action.
template<typename Func>
class CallDescImpl : public CallDesc {
public:
    // Construct a CallDescImpl.
    // @param func: The function to be invoked in invoke().
    CallDescImpl(Func func) : m_func(func) {}
private:
    // Invoke the function of the CallDescImpl.
    virtual void invoke() {
        m_func();
    }
    Func m_func;
};

// Represent the state of a remote call. There is one CallResult per CallDesc,
// templated on the type of the function ran by the CallDesc. A CallResult can
// be used to query whether or not the invocation of the remote function
// completed on the remote cpu and to get the value returned by the invocation,
// if any.
template<typename T>
class CallResult {
public:
    // FIXME: This type be non-copyable?

    // Check if the remote call associated with this instance was executed and
    // completed on the remote cpu.
    // @return: true if the call was completed, ie. the function was called and
    // returned, false otherwise.
    bool isDone() const {
        return m_done;
    }

    // BUSY-Wait for the remote call to be completed on the remote cpu. Only
    // returns once it completed.
    void wait() const {
        // FIXME: It would be good to have a wait() on Atomic<T> directly.
        while (!isDone()) {
            asm("pause");
        }
    }

    // Get the value returned by the associated remote call. If the result is
    // not currently available (ie. isDone() == false) this function first waits
    // for the call to be completed before returning the value.
    // @return: A copy of the value returned by the remote call.
    T const& returnValue() const {
        wait();
        return m_returnValue;
    }

private:
    Atomic<u8> m_done;
    T m_returnValue;

    // Give access to private members to invokeOn() in order for it to set the
    // m_done and m_returnValue.
    template<typename Func, typename... Args>
    friend auto invokeOn(Smp::Id const destCpu, Func func, Args&&... args)
        -> CallResult<decltype(func(args...))>*;
};

// Specialization of CallResult for a function that does not return a value,
// e.g. void return type. This is essentially the same as CallResult<T> except
// that there is no return value to be read.
template<>
class CallResult<void> {
public:
    // Check if the remote call associated with this instance was executed and
    // completed on the remote cpu.
    // @return: true if the call was completed, ie. the function was called and
    // returned, false otherwise.
    bool isDone() const {
        return m_done;
    }

    // BUSY-Wait for the remote call to be completed on the remote cpu. Only
    // returns once it completed.
    void wait() const {
        // FIXME: It would be good to have a wait() on Atomic<T> directly.
        while (!isDone()) {
            asm("pause");
        }
    }

private:
    Atomic<u8> m_done;

    // Give access to private members to invokeOn() in order for it to set the
    // m_done.
    template<typename Func, typename... Args>
    friend auto invokeOn(Smp::Id const destCpu, Func func, Args&&... args)
        -> CallResult<decltype(func(args...))>*;
};
}
