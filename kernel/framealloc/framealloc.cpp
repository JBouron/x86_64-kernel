#include <framealloc/framealloc.hpp>
#include <util/panic.hpp>
#include <util/assert.hpp>

#include "./allocator.hpp"

namespace FrameAlloc {

// Create a Frame from its physical offset.
// @param physicalOffset: The physical offset of the frame.
Frame::Frame(u64 const physicalOffset) : m_physicalOffset(physicalOffset) {}

// Get the physical offset of the frame.
u64 Frame::phyOffset() const {
    return m_physicalOffset;
}


// The current instance of the global frame allocator.
static Allocator* GLOBAL_ALLOCATOR = nullptr;

// Initialize the frame allocator.
// @param bootStruct: The bootStruct passed by the bootloader. The frame
// allocator is initialized from the bootStruct's physical frame free list.
void Init(BootStruct const& bootStruct) {
    if (!!GLOBAL_ALLOCATOR) {
        Log::warn("FrameAlloc::Init called twice, skipping");
        return;
    }
    static EarlyAllocator earlyAllocator(bootStruct);
    GLOBAL_ALLOCATOR = &earlyAllocator;
    Log::debug("Initialized early frame allocator");
}

void Test(__attribute((unused)) SelfTests::TestRunner& runner) {
    // TODO: Add frame alloc tests.
}

// Notify the frame allocator that the direct map has been initialized.
void directMapInitialized() {
    static bool embAllocatorInit = false;
    if (embAllocatorInit) {
        Log::warn("FrameAlloc::directMapInitialized called twice, skipping");
        return;
    }
    static EmbeddedFreeListAllocator embAllocator;
    // FIXME: Remove this cast.
    EarlyAllocator const* const earlyAlloc(
        static_cast<EarlyAllocator const*>(GLOBAL_ALLOCATOR));
    earlyAlloc->initEmbeddedFreeListAllocator(embAllocator);
    GLOBAL_ALLOCATOR = &embAllocator;
}

// Allocate a physical frame using the global allocator. This function panics if
// no frame can be allocated.
// @return: The Frame describing the allocated frame.
Frame alloc() {
    if (!GLOBAL_ALLOCATOR) {
        PANIC("Attempt to call FrameAlloc::alloc() before calling Init()!");
    }
    return GLOBAL_ALLOCATOR->alloc();
}

// Free an allocated physical frame.
// @param Frame: A Frame describing the physical frame to be freed.
void free(Frame const& frame) {
    if (!GLOBAL_ALLOCATOR) {
        PANIC("Attempt to call FrameAlloc::free() before calling Init()!");
    }
    GLOBAL_ALLOCATOR->free(frame);
}

}
