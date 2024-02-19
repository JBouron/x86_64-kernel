#include <memory/malloc.hpp>
#include <util/assert.hpp>
#include "heapallocator.hpp"
#include <logging/log.hpp>
#include <util/panic.hpp>
#include <concurrency/lock.hpp>

namespace HeapAlloc {
// Defined in linker script, address of the very last byte of the kernel in the
// virtual address space.
extern "C" u8 KERNEL_END_VADDR;

// Virtual address of the first byte available after the kernel.
static VirAddr const KERNEL_END = &KERNEL_END_VADDR;

// Start virtual address for the head. Make it start on the next page boundary
// after the kernel.
// FIXME: Having an "align-to" function in VirAddr could be useful.
static VirAddr const HEAP_START = KERNEL_END +
                                  PAGE_SIZE - (KERNEL_END.raw() % PAGE_SIZE);

// The maximum size of the heap. Only there as a safety net to detect when
// something goes wrong and the heap is monotically growing. The value should be
// high enough so that we never reach this limit except when hitting a bug/leak.
static u64 const HEAP_MAX_SIZE = 512 * PAGE_SIZE;

// The global heap allocator.
static HeapAllocator* HEAP_ALLOCATOR = nullptr;

// Lock to use the global heap allocator.
static Concurrency::SpinLock HEAP_ALLOC_LOCK;

// Has Init() been called already? Used to assert that cpus are not trying to
// use the namespace before its initialization.
static bool IsInitialized = false;

// Initialize the heap allocator. Must be called before calling alloc() and
// free() for the first and must be called after both paging and the frame
// allocator have been initialized.
void Init() {
    if (!!HEAP_ALLOCATOR) {
        // Heap allocator was already initialized, nothing to do here.
        Log::warn("HeapAlloc::Init() called twice, skipping");
        return;
    }
    Log::info("Initializing kernel heap starting {} for {} bytes",
              HEAP_START,
              HEAP_MAX_SIZE);
    static HeapAllocator heapAllocator(HEAP_START, HEAP_MAX_SIZE);
    HEAP_ALLOCATOR = &heapAllocator;
    IsInitialized = true;
}

// Allocate memory into the kernel heap.
// @param size: The number of bytes for the allocation.
// @return: On success a void pointer to the allocated memory, otherwise returns
// an error.
Res<void*> malloc(u64 const size) {
    ASSERT(IsInitialized);
    Concurrency::LockGuard guard(HEAP_ALLOC_LOCK);
    return HEAP_ALLOCATOR->alloc(size);
}

// Free memory from the heap that was allocated with a call to malloc().
// @param ptr: The pointer to be freed.
void free(void const * const ptr) {
    ASSERT(IsInitialized);
    Concurrency::LockGuard guard(HEAP_ALLOC_LOCK);
    HEAP_ALLOCATOR->free(ptr);
}
}

// new and delete operators definition. Those operators don't need to appear in
// a header file, the compiler just expects them to exist somewhere. Moreover
// they obviously need to be define at the top-level namespace.
// There is one shortcoming however: we cannot return errors as new and new[]
// must return a void*. So for now, if any allocation error occurs, raise a
// PANIC.
void *operator new(u64 const size) {
    Res<void*> const allocRes(HeapAlloc::malloc(size));
    if (!allocRes) {
        PANIC("Failed to allocate memory: {}", allocRes.error());
    } else {
        return *allocRes;
    }
}

void *operator new[](u64 const size) {
    Res<void*> const allocRes(HeapAlloc::malloc(size));
    if (!allocRes) {
        PANIC("Failed to allocate memory: {}", allocRes.error());
    } else {
        return *allocRes;
    }
}

void operator delete(void * const ptr) {
    HeapAlloc::free(ptr);
}

void operator delete[](void * const ptr) {
    HeapAlloc::free(ptr);
}

// Not sure what the second arg is used for, but we don't need anymore info to
// free anyway.
void operator delete(void * const ptr, __attribute__((unused)) u64 const sz) {
    HeapAlloc::free(ptr);
}

// Not sure what the second arg is used for, but we don't need anymore info to
// free anyway.
void operator delete[](void * const ptr, __attribute__((unused)) u64 const sz) {
    HeapAlloc::free(ptr);
}
