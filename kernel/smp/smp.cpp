// Functions related to multi-processor initialization.
#include <smp/smp.hpp>
#include <acpi/acpi.hpp>
#include <interrupts/interrupts.hpp>
#include <interrupts/lapic.hpp>
#include <timers/lapictimer.hpp>
#include <memory/segmentation.hpp>
#include <util/assert.hpp>
#include <memory/stack.hpp>
#include <util/panic.hpp>
#include <concurrency/atomic.hpp>
#include <paging/paging.hpp>

namespace Smp {

// Indicates whether or not the current cpu with is the BootStrap Processor
// (BSP), aka. BSC (BootStrap Core) in AMD's documentation.
// @return: true if this cpu is the BSP, false otherwise.
bool isBsp() {
    return !!(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE) & (1 << 8));
}

// Get the SMP ID of the current core.
// @return: The ID of the cpu making the call.
Id id() {
    // FIXME: For now we are using CPUID to read the ID. This is however a very
    // inefficient way to do so as this serializes the instruction stream.
    // Eventually we should "cache" the cpu ids in per-cpu data structures.
    Cpu::CpuidResult const res(Cpu::cpuid(0x01));
    u64 const id(res.ebx >> 24);
    return Id(id);
}

// Get the number of cpus in the system.
// @param: The number of cpus in the system, including the BSP.
u64 ncpus() {
    Acpi::Info const& acpi(Acpi::parseTables());
    return acpi.processorDescSize;
}

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
    Log::info("Waking cpu {}", id);

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

// Struct containing various info that is required to setup/configure an
// Application Processor (AP). Each AP, upon booting, checks this struct, stored
// at a known address in order to know which GDT, page-table, ... to use.
struct ApBootInfo {
    // A temporary GDT stored under the 1MiB limit, thus accessible from
    // real-mode, that the AP can use in order to transition to 64-bit mode.
    // Once in 64-bit mode, the AP switches over to the "real" GDT which is
    // store in higher-half virtual memory, see `finalGdtAddr`.
    // This temporary GDT contains the following entries:
    //  Index | Entry type
    //    0x1 | 32-bit flat code segment
    //    0x2 | 32-bit flat R/W data segment
    //    0x3 | 64-bit code segment
    //    0x4 | 64-bit data segment
    u64 gdt[5];
    // The page-table to be used by the AP when switching to 64-bit mode. This
    // is the same as used by the Boot-strap processor (BSP).
    u32 pageTable;
    // The virtual address to jump to after 64-bit has been enabled and the GDT
    // setup.
    u64 targetAddr;
} __attribute__((packed));
static_assert(sizeof(ApBootInfo) <= PAGE_SIZE);

// Start and end addresses of the AP startup code. Note that this code resides
// in higher-half addresses and needs to be copied an appropriate address in
// physical memory.
extern "C" u8 apStartup;
extern "C" u8 apStartupEnd;

// Flag used by APs to notify the waking cpu that they are online. The waking
// cpu waits on this flag before returning from startupApplicationProcessor.
Atomic<u8> apStartFlag;

// Startup an application processor, that is:
//  1. Wake the processor and transition from real-mode to 64-bit mode.
//  2. Use the same GDT and page table as the calling processor.
//  3. Allocate a stack for the application processor.
//  4. Branch execution to a specified location.
// This function returns once all four steps have completed. Due to the inherent
// nature of multi-threading, this function may return a slight amount of time
// before the AP actually jumps to the specified location.
// @param id: The ID of the application processor to start.
// @param entryPoint: The 64-bit entry point to which the application processor
// branches to once awaken.
void startupApplicationProcessor(Id const id, void (*entryPoint64Bits)(void)) {
    // For now we only wake one application processor (AP) at a time, this makes
    // things easier due to not requiring any mutex/lock. All APs can use the
    // same physical frame as their boot stack.

    // The AP booting "protocol" uses three physical frames:
    //  - A code frame where the boot is loaded: 0x8000.
    //  - A data frame where the booting AP can find the ApBootInfo that
    //  contains the various info required to setup/configure the AP: 0x9000.
    //  - A stack frame on which the booting AP sets up a temporary stack used
    //  until it can allocate a proper stack: 0xa000.
    // All of the frames above have hard-coded addresses because the frame
    // allocator does not allow us to force an allocation to be under a specific
    // address. Moreover, some addresses lead to invalid vectors when waking an
    // AP (see comment in wakeApplicationProcessor). Hence we re-use frames that
    // were used by the bootloader.
    Log::debug("Starting application processor {}", id);

    PhyAddr const apStartupCodeFrame(0x8000);
    PhyAddr const apBootInfoFrame(0x9000);
    PhyAddr const apStackFrame(0xa000);

    // Prepare the ApBootInfo struct.
    ApBootInfo * const bootInfo(apBootInfoFrame.toVir().ptr<ApBootInfo>());

    bootInfo->gdt[0] = Memory::Segmentation::Descriptor().raw();
    // 32-bit code segment.
    bootInfo->gdt[1] = Memory::Segmentation::Descriptor32Flat(
        Cpu::PrivLevel::Ring0,
        Memory::Segmentation::Descriptor::Type::CodeExecuteReadable).raw();
    // 32-bit data segment.
    bootInfo->gdt[2] = Memory::Segmentation::Descriptor32Flat(
        Cpu::PrivLevel::Ring0,
        Memory::Segmentation::Descriptor::Type::DataReadWrite).raw();
    // 64-bit code segment.
    bootInfo->gdt[3] = Memory::Segmentation::Descriptor64(
        Cpu::PrivLevel::Ring0,
        Memory::Segmentation::Descriptor::Type::CodeExecuteReadable).raw();
    // 64-bit data segment.
    bootInfo->gdt[4] = Memory::Segmentation::Descriptor64(
        Cpu::PrivLevel::Ring0,
        Memory::Segmentation::Descriptor::Type::DataReadWrite).raw();

    bootInfo->pageTable = Cpu::cr3();

    bootInfo->targetAddr = reinterpret_cast<u64>(entryPoint64Bits);

    // Copy the AP startup code to the apStartupCodeFrame. We must do this
    // because currently this code resides in higher-half mapping which is of
    // course inaccessible from real-mode. We cannot modify the linking of the
    // boot-code to be loaded by the bootloader at the desired address as we are
    // effectively overwriting the bootloader.
    VirAddr const codeStart(&apStartup);
    VirAddr const codeEnd(&apStartupEnd);
    ASSERT(codeStart < codeEnd);
    u64 const codeLen(codeEnd - codeStart);
    Util::memcpy(apStartupCodeFrame.toVir().ptr<void>(),
                 codeStart.ptr<void>(),
                 codeLen);

    // Wake the AP.
    apStartFlag = 0;
    wakeApplicationProcessor(id, apStartupCodeFrame);

    // Wait for the AP to be online.
    while (!apStartFlag) {
        asm("pause");
    }
}

// Function called by an application processor during the startup routine, after
// switching to 64-bits mode.
// This function finalizes the AP configuration, ie. switches to the final GDT,
// allocates a stack, ... before jumping to the target indicated in the
// ApBootInfo struct. The goal is to keep apStartup as small as possible by
// doing anything that can be done in C++ here.
// Defined as extern "C" so that it is callable from asm.
// This function does NOT return.
// @param info: The ApBootInfo.
extern "C" void finalizeApplicationProcessorStartup(
    ApBootInfo const * const info) {

    // Switch to the final GDT that will be used until reset.
    Memory::Segmentation::InitCurrCpu();

    // Load the kernel-wide IDT.
    Interrupts::InitCurrCpu();

    // Finish paging configuration.
    Paging::InitCurrCpu();

    // Configure this cpu's LAPIC.
    Interrupts::lapic();

    // Allocate a stack for this cpu.
    Res<VirAddr> const stackAllocRes(Stack::allocate());
    if (!stackAllocRes) {
        PANIC("Could not allocate a stack for AP {}, reason: {}",
            Smp::id(), stackAllocRes.error());
    }

    // Use a trampoline to avoid a race condition when waking APs one after the
    // others. This trampoline makes sure that the AP writes into apStartFlag
    // _after_ switching to its new stack. Without a trampoline there would be a
    // small window of time between the time apStartFlag = 1 and the time when
    // the AP switch to the new stack where the AP still uses the old stack used
    // for the startup code. If another AP is awaken during that time, both AP
    // end-up using the same stack!
    auto const trampoline([](u64 const arg) {
        // We need to read the ApBootInfo _before_ setting the apStartFlag to 1,
        // as ApBootInfo, more importantly its targetAddr might get overwritten
        // after setting the flag!
        ApBootInfo const * const info(reinterpret_cast<ApBootInfo const*>(arg));
        void (*target)() = reinterpret_cast<void(*)()>(info->targetAddr);

        // Notify the cpu waking this AP that it is online.
        apStartFlag = 1;

        target();
        PANIC("Cpu {} returned from the target passed in wakeAP!");
    });

    // Switch to the new stack and jump to the target.
    Stack::switchToStack(stackAllocRes.value(),
                         trampoline,
                         reinterpret_cast<u64>(info));
    UNREACHABLE
}
}
