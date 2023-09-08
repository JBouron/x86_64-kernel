// Functions related to multi-processor initialization.
#include <smp/smp.hpp>
#include <acpi/acpi.hpp>
#include <interrupts/lapic.hpp>
#include <timers/lapictimer.hpp>

namespace Smp {

// Send an INIT IPI to a remote CPU. This function takes care of checking if the
// INIT IPI was successfully delivered and re-try the IPI if necessary.
// @param id: The ID of the target cpu.
static void sendInitIpi(Id const id) {
    // The INIT IPI to send to the target cpu.
    Interrupts::Lapic::InterruptCmd const initIcr({
        .vector = Interrupts::Vector(0),
        .messageType = Interrupts::Lapic::InterruptCmd::MessageType::Init,
        .destination = static_cast<u8>(id.raw())
    });

    // The max number of tries.
    static u64 const maxTries = 10;
    for (u64 i(0); i < maxTries; ++i) {
        // Send the IPI.
        Log::debug("Sending IPI to cpu {} (attempt {}/{})",
                   id.raw(), i + 1, maxTries);
        Interrupts::lapic().setInterruptCommand(initIcr);

        // The Multiprocessor Specification indicates that under normal
        // operation the IPI should be delivered in less than 20us.
        Timer::LapicTimer::delay(Timer::Duration::MicroSecs(20));

        // Check if the IPI was successfully delivered, this is indicated by a
        // reset of the deliveryStatus in the ICR.
        if (!Interrupts::lapic().interruptCommand().deliveryStatus) {
            // The IPI has been successfully delivered.
            return;
        } else {
            Log::warn("INIT IPI was not delivered to cpu {} after 20 us,"
                      " retrying", id.raw());
        }
    }
    PANIC("Failed to send INIT IPI to cpu {} after {} tries",
          id.raw(), maxTries);
}

// Send an STARTUP IPI to a remote CPU. This function takes care of checking if
// the STARTUP IPI was successfully delivered and re-try the IPI if necessary.
// @param id: The ID of the target cpu.
// @param vector: The vector to include in the SIPI.
static void sendStartupIpi(Id const id, Interrupts::Vector const vector) {
    // The INIT IPI to send to the target cpu.
    Interrupts::Lapic::InterruptCmd const startupIcr({
        .vector = vector,
        .messageType = Interrupts::Lapic::InterruptCmd::MessageType::Startup,
        .destination = static_cast<u8>(id.raw())
    });

    // The max number of tries.
    static u64 const maxTries = 10;
    for (u64 i(0); i < maxTries; ++i) {
        // Send the SIPI.
        Log::debug("Sending STARTUP IPI to cpu {} (attempt {}/{})",
                   id.raw(), i + 1, maxTries);
        Interrupts::lapic().setInterruptCommand(startupIcr);

        // The Multiprocessor Specification indicates that under normal
        // operation the IPI should be delivered in less than 20us.
        Timer::LapicTimer::delay(Timer::Duration::MicroSecs(20));

        // Check if the IPI was successfully delivered, this is indicated by a
        // reset of the deliveryStatus in the ICR.
        if (!Interrupts::lapic().interruptCommand().deliveryStatus) {
            // The IPI has been successfully delivered.
            return;
        } else {
            Log::warn("STARTUP IPI was not delivered to cpu {} after 20 us,"
                      " retrying", id.raw());
        }
    }
    PANIC("Failed to send STARTUP IPI to cpu {} after {} tries",
          id.raw(), maxTries);
}

// Wake an Application Processors (AP).
// @param id: The ID of the cpu to be woken.
// @param bootStrapRoutine: Physical address of the bootstrap routine at which
// the APs should start executing. This must point to real-mode code. This
// address must be page aligned and below the 1MiB address.
void wakeApplicationProcessor(Id const id, PhyAddr const bootStrapRoutine) {
    // Booting APs is somewhat CPU dependent, because otherwise where would be
    // the fun?? The best documentation regarding AP startup can be found in the
    // Intel Multiprocessor Specification:
    //  https://web.archive.org/web/20170410220205/https://download.intel.com
    //  /design/archives/processors/pro/docs/24201606.pdf
    // (Note: The above should be a single line).
    //
    // The specification above gives a "universal" algorithm to boot APs. It
    // turns out there are two main flavors of this algorithm depending on
    // wether or nor the CPU uses a _discrete_ APIC (aka 82489DX) or
    // _integrated_ APIC.
    // Because we assume that we are running on a somewhat recent CPU we will
    // only support the case of _integrated_ APIC, which is much simpler to
    // support/boot anyway.
    Log::info("Waking cpu {}", id.raw());

    // Check the version register of the LAPIC to confirm that we are dealing
    // with an integrated APIC.
    Interrupts::Lapic::VersionInfo const vInfo(Interrupts::lapic().version());
    bool const hasIntegratedApic(!!(vInfo.version & 0xf0));
    if (!hasIntegratedApic) {
        PANIC("This kernel only support waking AP on CPU with integrated APIC");
    }

    // The algorithm to wake an AP with integrated APIC is as follows:
    //   1. Send INIT IPI
    //   2. Wait 10 ms.
    //   3. Send first STARTUP IPI (SIPI).
    //   4. Wait 200 us.
    //   5. Send second STARTUP IPI (SIPI).
    //   6. Wait 200 us.
    // In reality in turns out that a single SIPI is sufficient! It is not
    // really clear why or if the second one is necessary.
    // Worse yet: the second SIPI could wake the AP a second time, this can lead
    // to all sorts of issues, for instance double counting CPUs.
    // Therefore in this kernel we only send a single SIPI.
    //
    // The SIPI contains the address of the startup code to be jumped to by the
    // AP in its vector field. For a SIPI with vector = 0xVV (remember, limited
    // to 8-bits), the AP starts execution at physical address 0xVV000, in
    // real-mode, with CS:IP = VV00:0x0000. But wait, there's more: we are not
    // allowed to use vectors in range 0xA0-0xBF (this is only mentionned in the
    // Multiproc spec, NOT the Intel64 spec...), this is most likely due to
    // video display memory being mapped in range 0xa0000-0xbffff.

    // Check that the bootstrap routine address is actually valid.
    if (!bootStrapRoutine.isPageAligned()) {
        PANIC("Bootstrap routine must be page aligned");
    } else if (bootStrapRoutine >= (1 << 20)) {
        PANIC("Bootstrap routine must be < 1MiB limit");
    } else if(!!(bootStrapRoutine.raw() & ~(0xff000))) {
        // The bootstrap routine address is encoded in the vector field of the
        // ICR and as such only 8-bits are available.
        PANIC("Bootstrap routine's offset does not fit in ICR vector field");
    }

    // Compute the vector number that will be used in the SIPI.
    Interrupts::Vector const sipiVector(bootStrapRoutine.raw() >> 12);
    // Check that the vector is allowed.
    if (0xa0 <= sipiVector.raw() && sipiVector.raw()  <= 0xbf) {
        Log::crit("SIPI vector in range 0xa0-0xbf is not allowed");
        PANIC("Bootstrap code address {} is not allowed", bootStrapRoutine);
    }

    // Check that the id is referring to an existing cpu and that this cpu can
    // be woken up per the ACPI tables.
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

    // We are ready to wake the remote cpu now. One last thing to keep in mind:
    // the INIT and SIPI IPIs don't have the same delivery guarantees as
    // "regular" IPIs: the OS needs to manually check that the IPI and SIPI were
    // successfully delivered and retry if necessary. This is handled by
    // sendInitIpi() and sendStartupIpi().

    // Send the INIT IPI.
    sendInitIpi(id);

    // Wait 10 milliseconds.
    Timer::LapicTimer::delay(Timer::Duration::MilliSecs(10));

    // Send the Startup IPI.
    sendStartupIpi(id, sipiVector);
    // At this point the core should be running.
}

}
