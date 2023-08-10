// Malloc-like heap allocator
#pragma once
#include <util/result.hpp>

namespace HeapAlloc {

// Initialize the heap allocator. Must be called before calling alloc() and
// free() for the first and must be called after both paging and the frame
// allocator have been initialized.
// FIXME: We need a way to enforce initialization orders.
void Init();

// Allocate memory into the kernel heap.
// @param size: The number of bytes for the allocation.
// @return: On success a void pointer to the allocated memory, otherwise returns
// an error.
Res<void*> malloc(u64 const size);

// Free memory from the heap that was allocated with a call to malloc().
// @param ptr: The pointer to be freed.
void free(void const * const ptr);

// Run heap allocation tests.
void Test(SelfTests::TestRunner& runner);

}
