// Functions related to multi-processor initialization.
#pragma once
#include <util/subrange.hpp>
#include <util/addr.hpp>
#include <selftests/selftests.hpp>

namespace Smp {

// The ID of a cpu. For now we only support up to 256 cpus due to the fact that
// we don't yet support x2APIC.
class Id : public SubRange<Id, 0, 255> {};

// Wake an Application Processors (AP).
// @param id: The ID of the cpu to be woken.
// @param bootStrapRoutine: Physical address of the bootstrap routine at which
// the APs should start executing. This must point to real-mode code. This
// address must be page aligned and below the 1MiB address.
void wakeApplicationProcessor(Id const id, PhyAddr const bootStrapRoutine);

// Run SMP tests.
void Test(SelfTests::TestRunner& runner);
}
