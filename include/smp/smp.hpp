// Functions related to multi-processor initialization.
#pragma once
#include <util/subrange.hpp>
#include <selftests/selftests.hpp>

namespace Smp {

// The ID of a cpu. For now we only support up to 256 cpus due to the fact that
// we don't yet support x2APIC.
class Id : public SubRange<Id, 0, 255> {};

// Indicates whether or not the current cpu with is the BootStrap Processor
// (BSP), aka. BSC (BootStrap Core) in AMD's documentation.
// @return: true if this cpu is the BSP, false otherwise.
bool isBsp();

// Get the SMP ID of the current core.
// @return: The ID of the cpu making the call.
Id id();

// Get the number of cpus in the system.
// @param: The number of cpus in the system, including the BSP.
u64 ncpus();

// Startup an application processor, that is:
//  1. Wake the processor and transition from real-mode to 64-bit mode.
//  2. Use the same GDT and page table as the calling processor.
//  3. Allocate a stack for the application processor.
//  4. Branch execution to a specified location.
// This function returns once all four steps have completed.
// @param id: The ID of the application processor to start.
// @param entryPoint: The 64-bit entry point to which the application processor
// branches to once awaken.
void startupApplicationProcessor(Id const id, void (*entryPoint64Bits)(void));

// Run SMP tests.
void Test(SelfTests::TestRunner& runner);
}
