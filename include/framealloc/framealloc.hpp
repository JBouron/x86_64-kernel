// Everything related to physical page frame allocation.

#pragma once

#include <util/util.hpp>
#include <util/result.hpp>
#include <bootstruct.hpp>
#include <paging/paging.hpp>
#include <selftests/selftests.hpp>
#include <util/addr.hpp>

namespace FrameAlloc {

// Describes a physical frame.
class Frame {
public:
    // Default constructor. The resulting physical offset is 0x0.
    Frame();

    // Create a Frame from its physical address.
    // @param physicalAddr: The physical address of the frame.
    Frame(PhyAddr const physicalOffset);

    bool operator==(Frame const& other) const = default;

    // Get the physical address of the frame.
    PhyAddr addr() const;

private:
    PhyAddr m_physicalAddr;
};

// Initialize the frame allocator.
// @param bootStruct: The bootStruct passed by the bootloader. The frame
// allocator is initialized from the bootStruct's physical frame free list.
void Init(BootStruct const& bootStruct);

// Run the frame allocation tests.
void Test(SelfTests::TestRunner& runner);

// Notify the frame allocator that the direct map has been initialized.
void directMapInitialized();

// Allocate a physical frame using the global allocator. This function panics if
// no frame can be allocated.
// @return: The Frame describing the allocated frame. If the allocation failed
// return an error instead.
Res<Frame> alloc();

// Free an allocated physical frame.
// @param Frame: A Frame describing the physical frame to be freed.
void free(Frame const& frame);

}

// Shortcut to avoid long typenames.
using Frame = FrameAlloc::Frame;
