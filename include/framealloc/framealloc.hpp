// Everything related to physical page frame allocation.

#pragma once

#include <util/util.hpp>
#include <bootstruct.hpp>

// The size of a page in bytes. For now this kernel only supports the default
// size of 4096 bytes pages.
static constexpr u64 PAGE_SIZE = 0x1000;

namespace FrameAlloc {

// Describes a physical frame.
class Frame {
public:
    // Create a Frame from its physical offset.
    // @param physicalOffset: The physical offset of the frame.
    Frame(u64 const physicalOffset);

    // Get the physical offset of the frame.
    u64 phyOffset() const;

private:
    u64 const m_physicalOffset;
};

// Initialize the frame allocator.
// @param bootStruct: The bootStruct passed by the bootloader. The frame
// allocator is initialized from the bootStruct's physical frame free list.
void Init(BootStruct const& bootStruct);

// Allocate a physical frame using the global allocator. This function panics if
// no frame can be allocated.
// @return: The Frame describing the allocated frame.
Frame alloc();

// Free an allocated physical frame.
// @param Frame: A Frame describing the physical frame to be freed.
void free(Frame const& frame);

}

// Shortcut to avoid long typenames.
using Frame = FrameAlloc::Frame;
