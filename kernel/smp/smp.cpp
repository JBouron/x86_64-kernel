// Functions related to multi-processor initialization.
#include <smp/smp.hpp>
#include <acpi/acpi.hpp>
#include <interrupts/lapic.hpp>
#include <timers/lapictimer.hpp>

namespace Smp {

// Wake an Application Processors (AP).
// @param id: The ID of the cpu to be woken.
// @param bootStrapRoutine: Physical address of the bootstrap routine at which
// the APs should start executing. This must point to real-mode code. This
// address must be page aligned and below the 1MiB address.
void wakeApplicationProcessor(Id const id, PhyAddr const bootStrapRoutine) {
    // Check that the bootstrap address is valid.
    if (!bootStrapRoutine.isPageAligned()) {
        PANIC("Bootstrap routine must be page aligned");
    } else if (bootStrapRoutine >= (1 << 20)) {
        PANIC("Bootstrap routine must be < 1MiB limit");
    } else if(!!(bootStrapRoutine.raw() & ~(0xff000))) {
        // The bootstrap routine address is encoded in the vector field of the
        // ICR and as such only 8-bits are available.
        PANIC("Bootstrap routine's offset does not fit in ICR vector field");
    }

    // Check that the id is referring to an existing cpu and that this cpu can
    // be woken up.
    Acpi::Info const& acpiInfo(Acpi::parseTables());
    // Was the cpu found in the ACPI tables?
    bool cpuFound(false);
    for (u64 i(0); i < acpiInfo.processorDescSize; ++i) {
        Acpi::Info::ProcessorDesc const& cpuDesc(acpiInfo.processorDesc[i]);
        if (id != cpuDesc.id) {
            // Not this cpu.
            continue;
        } else if (!cpuDesc.isEnabled && !cpuDesc.isOnlineCapable) {
            // The cpu might be disabled, don't try to wake it up.
            PANIC("CPU {} is marked as non-online-capable in ACPI tables",
                  id.raw());
        } else {
            cpuFound = true;
        }
    }
    if (!cpuFound) {
        PANIC("CPU {} does not exist, cannot wake it", id.raw());
    }

    // We are ready to wake the remote cpu. We _roughly_ follow Intel's manual
    // steps from Vol3 Chapter 8.4.4.1 Typical BSP Initialization Sequence, here
    // with one difference: we send a single SIPI because it seems to be enough
    // in practice.

    // Send the INIT IPI.
    Interrupts::Lapic::InterruptCmd const initIcr({
        .vector = Interrupts::Vector(0),
        .messageType = Interrupts::Lapic::InterruptCmd::MessageType::Init,
        .destination = static_cast<u8>(id.raw())
    });
    Interrupts::lapic().setInterruptCommand(initIcr);

    // Wait 10 milliseconds.
    Timer::LapicTimer::delay(Timer::Duration::MilliSecs(10));

    // Send the Startup IPI.
    Interrupts::Lapic::InterruptCmd const startupIcr({
        .vector = Interrupts::Vector(bootStrapRoutine.raw() >> 12),
        .messageType = Interrupts::Lapic::InterruptCmd::MessageType::Startup,
        .destination = static_cast<u8>(id.raw())
    });
    Interrupts::lapic().setInterruptCommand(startupIcr);
    // At this point the core should be running.
}

}
