#include <framealloc/framealloc.hpp>
#include <util/panic.hpp>
#include <util/assert.hpp>

#include "./allocator.hpp"

namespace FrameAlloc {

// Default constructor. The resulting physical offset is 0x0.
Frame::Frame() : Frame(0x0) {}

// Create a Frame from its physical address.
// @param physicalAddr: The physical address of the frame.
Frame::Frame(PhyAddr const physicalAddr) : m_physicalAddr(physicalAddr) {}

// Get the physical address of the frame.
PhyAddr Frame::addr() const {
    return m_physicalAddr;
}


// The current instance of the global frame allocator.
static Allocator* GLOBAL_ALLOCATOR = nullptr;

// Has Init() been called already? Used to assert that cpus are not trying to
// use the namespace before its initialization.
static bool IsInitialized = false;

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
    IsInitialized = true;
}

// Notify the frame allocator that the direct map has been initialized.
void directMapInitialized() {
    ASSERT(IsInitialized);
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
// @return: The Frame describing the allocated frame. If the allocation failed
// return an error instead.
Res<Frame> alloc() {
    ASSERT(IsInitialized);
    return GLOBAL_ALLOCATOR->alloc();
}

// Free an allocated physical frame.
// @param Frame: A Frame describing the physical frame to be freed.
void free(Frame const& frame) {
    ASSERT(IsInitialized);
    GLOBAL_ALLOCATOR->free(frame);
}

}
