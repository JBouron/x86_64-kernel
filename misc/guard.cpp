// This file defines the necessary guard functions expected by GCC when
// declaring local static variables to avoid race condition during
// initialization/construction.
#include <util/ints.hpp>
#include <concurrency/lock.hpp>

namespace __cxxabiv1 
{
    // C++ ABI requires a 64-bit type for the guard.
	using __guard = u64;

    // static variable requiring constructors are not expected to arise very
    // often (this is usually a code smell anyway). Therefore to keep things
    // simple we use a single system-wide lock to control all the
    // guards/static-init.
    Concurrency::SpinLock guardLock;
 
    // This should return 1 if the object under the guard needs to be
    // initialized, 0 otherwise. The lock must be held during the entire
    // initialization of the object. If multiple threads are trying to
    // initialize the same object at the same time, then one thread is expected
    // to return 1, to initialize the object, while the other is only expected
    // to return 0 from this function _after_ the first thread was done
    // initializing the object.
	extern "C" int __cxa_guard_acquire (__guard *g) {
        guardLock.lock();
		bool const needInitialize(*g == 0);
        if (!needInitialize) {
            // The object was already initialized, hence it won't be initialized
            // again and __cxa_guard_release won't be called on this object.
            // Unlock the lock to let other static-init start.
            guardLock.unlock();
        }
        return needInitialize;
	}
 
	extern "C" void __cxa_guard_release (__guard *g) {
		*g = 1;
        guardLock.unlock();
	}
 
	extern "C" void __cxa_guard_abort (__guard *) {
 
	}
}
