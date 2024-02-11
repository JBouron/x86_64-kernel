// Per-cpu data.
#include <smp/percpu.hpp>
#include <datastruct/vector.hpp>
#include <acpi/acpi.hpp>
#include <logging/log.hpp>
#include <cpu/cpu.hpp>

namespace Smp::PerCpu {

static Vector<Data> perCpuDataVec;

// Initialize per-cpu data structures. This allocates one Data per cpu in the
// system. Requires the heap allocator.
void Init() {
    Acpi::Info const& acpi(Acpi::parseTables());
    u8 const numCpus(acpi.processorDescSize);
    Log::info("Init PerCpu");
    Log::debug("Allocating {} PerCpu::Data", numCpus);
    for (u8 i(0); i < numCpus; ++i) {
        perCpuDataVec.pushBack(Data());
    }
}

// Get a reference to the per-cpu data of the current cpu.
// @return: A non-const reference to this cpu's Data instance.
Data& data() {
    // FIXME: Smp::id() relies on executing the CPUID instruction to get this
    // cpu id which is extremely slow.
    return perCpuDataVec[Smp::id().raw()];
}

// Get a reference to the per-cpu data of the cpu with the given Id.
// @return: A non-const reference to cpu `cpuId`'s Data instance.
Data& data(Id const& cpuId) {
    return perCpuDataVec[cpuId.raw()];
}
}
