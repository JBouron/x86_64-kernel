// Per-cpu data.
#pragma once
#include <smp/smp.hpp>

namespace Smp::PerCpu {

// Hold per-cpu data, there is one instance of such struct per cpu in the
// system. Cpus can access their or other cpu's Data through data() and
// data(id).
struct Data {
};
// This struct must be packed as it can be accessed directly from assembly.

// Initialize per-cpu data structures. This allocates one Data per cpu in the
// system. Requires the heap allocator.
void Init();

// Get a reference to the per-cpu data of the current cpu.
// @return: A non-const reference to this cpu's Data instance.
Data& data();

// Get a reference to the per-cpu data of the cpu with the given Id.
// @return: A non-const reference to cpu `cpuId`'s Data instance.
Data& data(Id const& cpuId);
}
