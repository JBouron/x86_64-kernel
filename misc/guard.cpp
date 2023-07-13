// This file defines the necessary guard functions expected by GCC when
// declaring local static variables to avoid race condition during
// initialization/construction.
// For now this is taken from osdev.org, see:
//      https://wiki.osdev.org/C%2B%2B#Local_Static_Variables_.28GCC_Only.29
//
// The current implementation is obviously not thread safe. For now this is
// fine, as we do not have multi-cores or multi-threading yet. Eventually this
// should be using proper mutexes.

namespace __cxxabiv1 
{
	__extension__ typedef int __guard __attribute__((mode(__DI__)));
 
	extern "C" int __cxa_guard_acquire (__guard *g) {
		return !*(char *)(g);
	}
 
	extern "C" void __cxa_guard_release (__guard *g) {
		*(char *)g = 1;
	}
 
	extern "C" void __cxa_guard_abort (__guard *) {
 
	}
}
