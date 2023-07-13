// This file defines the __cxa_atexit function which GCC expects to exists. This
// function is responsible to register all destructors that should be called
// when a shared library needs to be unloaded. AFAIK there is no way to tell GCC
// not to use it (I did not bother to search very much), defining it here seems
// easy enough ...

// This function is expected to return 0 on success. Really we don't do anything
// here because we don't even call global destructor, there is no point doing
// that in a kernel.
extern "C" int __cxa_atexit(void (*destructor) (void *), void *arg, void *dso) {
    // Just here to avoid unused args warning.
    (void)destructor;
    (void)arg;
    (void)dso;

    return 0;
}
