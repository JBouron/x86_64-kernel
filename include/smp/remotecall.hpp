// Execute a function on a remote CPU.
#pragma once
#include <smp/smp.hpp>
#include <smp/percpu.hpp>
#include <smp/remotecalltypes.hpp>
#include <interrupts/vectormap.hpp>
#include <selftests/selftests.hpp>
#include <util/ptr.hpp>

namespace Smp::RemoteCall {

// Initialize the RemoteCall subsystem.
void Init();

// Invoke a function on a remote cpu. The function may be a static function or a
// lamdba with or without a capture list. Optionally the function may take
// arguments and return a value.
// Functions to be executed by the remote cpu are enqueued prior to interrupting
// the cpu. Functions are always invoked in the same order they were enqueued.
// @param destCpu: The cpu on which to invoke the function.
// @param func: The function to invoke.
// @param args...: The arguments to pass to the function.
// @return: A smart pointer to a CallResult<R> where R is the return type of the
// function (potentially void). This CallResult can be used by the caller to
// wait for the invocation to complete on the remote cpu and retreive the value
// returned by the invocation. A caller can safely ignore the returned pointer
// if it is not interested in either waiting for completion of the call or its
// result.
template<typename Func, typename... Args>
auto invokeOn(Smp::Id const destCpu, Func func, Args&&... args)
    -> Ptr<CallResult<decltype(func(args...))>> {

    Ptr<CallResult<decltype(func(args...))>> const res(
        Ptr<CallResult<decltype(func(args...))>>::New());

    // Encapsulate the call to func into a lambda taking no arguments and
    // returning no value. This is so that we only need to support running
    // void(*)(void) functions on remote cpus. See the comments in CallDescImpl.
    // Note: Using a copy-capture list is mandatory for two reasons:
    //  1. Avoid dangling references to args... when the remote cpu invokes the
    //  lambda.
    //  2. Avoid `res` to be de-allocated. By copying the smart pointer we
    //  create a new reference/smart-ptr with the same lifetime as the wrapper
    //  lambda.
    auto const wrapperLambda([=]() {
        if constexpr (!SameAs<decltype(func(args...)), void>) {
            auto const returnVal(func(args...));
            res->m_returnValue = returnVal;
        } else {
            func(args...);
        }
        res->m_done++;
    });

    // Enqueue a CallDesc into the remoteCallQueue of the destination cpu. The
    // remote cpu will free this object once the invocation returns.
    auto const callDesc(
        Ptr<CallDescImpl<decltype(wrapperLambda)>>::New(wrapperLambda));
    {
        Smp::PerCpu::Data& data(Smp::PerCpu::data(destCpu));
        Concurrency::LockGuard guard(data.remoteCallQueueLock);
        data.remoteCallQueue.pushBack(callDesc);
    }

    // Interrupt the remote cpu to make it process its remoteCallQueue.
    Interrupts::Ipi::sendIpi(destCpu, Interrupts::VectorMap::RemoteCallVector);
    return res;
}

// Execute remote call tests.
void Test(SelfTests::TestRunner& runner);
}
