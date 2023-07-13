// This file contains the definition of __cxa_pure_virtual() which GCC expects.
// This function is only called when a call to a pure virtual function could not
// be made, which is virtually never unless something horribly wrong happened.

extern "C" void __cxa_pure_virtual() {
    // FIXME: Once we can we should PANIC here.
}
