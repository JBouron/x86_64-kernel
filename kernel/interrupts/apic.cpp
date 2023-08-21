// Functions and types related to the Advanced Programmable Interrupt Controller
// (APIC).
#include "apic.hpp"
#include <paging/paging.hpp>
#include <cpu/cpu.hpp>
#include <util/assert.hpp>

namespace Interrupts {
namespace Apic {

// The virtual address of the base of the local APIC. Assigned upon calling
// Init.
static VirAddr LocalApicBase = 0;

// All APIC registers.
enum class Register : u64 {
    ApicId                                  = 0x020,
    ApicVersion                             = 0x030,
    TaskPriority                            = 0x080,
    ArbitrationPriority                     = 0x090,
    ProcessorPriority                       = 0x0A0,
    EndOfInterrupt                          = 0x0B0,
    RemoteRead                              = 0x0C0,
    LogicalDestination                      = 0x0D0,
    DestinationFormat                       = 0x0E0,
    SpuriousInterruptVector                 = 0x0F0,
    InService31to0                          = 0x100,
    InService63to32                         = 0x110,
    InService95to64                         = 0x120,
    InService127to96                        = 0x130,
    InService159to128                       = 0x140,
    InService191to160                       = 0x150,
    InService223to192                       = 0x160,
    InService255to224                       = 0x170,
    TriggerMode31to0                        = 0x180,
    TriggerMode63to32                       = 0x190,
    TriggerMode95to64                       = 0x1a0,
    TriggerMode127to96                      = 0x1b0,
    TriggerMode159to128                     = 0x1c0,
    TriggerMode191to160                     = 0x1d0,
    TriggerMode223to192                     = 0x1e0,
    TriggerMode255to224                     = 0x1f0,
    InterruptRequest31to0                   = 0x200,
    InterruptRequest63to32                  = 0x210,
    InterruptRequest95to64                  = 0x220,
    InterruptRequest127to96                 = 0x230,
    InterruptRequest159to128                = 0x240,
    InterruptRequest191to160                = 0x250,
    InterruptRequest223to192                = 0x260,
    InterruptRequest255to224                = 0x270,
    ErrorStatus                             = 0x280,
    InterruptCommandLow                     = 0x300,
    InterruptCommandHigh                    = 0x310,
    TimerLocalVectorTableEntry              = 0x320,
    ThermalLocalVectorTableEntry            = 0x330,
    PerformanceCounterLocalVectorTableEntry = 0x340,
    LocalInterrupt0VectorTableEntry         = 0x350,
    LocalInterrupt1VectorTableEntry         = 0x360,
    ErrorVectorTableEntry                   = 0x370,
    TimerInitialCount                       = 0x380,
    TimerCurrentCount                       = 0x390,
    TimerDivideConfiguration                = 0x3E0,
};

// Read a register from the local APIC.
// @param reg: The register to read.
// @return: The current value of the register.
static u32 readRegister(Register const reg) {
    if (reg == Register::EndOfInterrupt) {
        // The EOI register is write-only.
        PANIC("Attempt to read the EIO APIC register, which is write-only");
    }
    return *(LocalApicBase + static_cast<u64>(reg)).ptr<u32>();
}

// Check if a local APIC register can be written to, e.g. not read-only.
// @param reg: The register to check.
// @return: true if the register is writtable, false otherwise.
static bool isRegisterWritable(Register const reg) {
    return reg == Register::ApicId
           || reg == Register::TaskPriority
           || reg == Register::EndOfInterrupt
           || reg == Register::LogicalDestination
           || reg == Register::DestinationFormat
           || reg == Register::SpuriousInterruptVector
           || reg == Register::InterruptCommandLow
           || reg == Register::InterruptCommandHigh
           || reg == Register::TimerLocalVectorTableEntry
           || reg == Register::ThermalLocalVectorTableEntry
           || reg == Register::PerformanceCounterLocalVectorTableEntry
           || reg == Register::LocalInterrupt0VectorTableEntry
           || reg == Register::LocalInterrupt1VectorTableEntry
           || reg == Register::ErrorVectorTableEntry
           || reg == Register::TimerInitialCount
           || reg == Register::TimerCurrentCount
           || reg == Register::TimerDivideConfiguration;
}

// Write a register of the local APIC. Any attempt to write into a read-only
// register leads to a PANIC.
// @param reg: The register to write.
// @param value: The value to write.
static void writeRegister(Register const reg, u32 const value) {
    u64 const registerOff(static_cast<u64>(reg));
    if (!isRegisterWritable(reg)) {
        PANIC("Attempt to write into read-only APIC register {}", registerOff);
    }
    *(LocalApicBase + registerOff).ptr<u32>() = value;
}

// Initialize the APIC.
void Init() {
    // Check that the CPU supports APIC. It is getting hard to find one that
    // doesn't as of today.
    bool const isApicSupported(Cpu::cpuid(0x1).edx & (1 << 9));
    if (!isApicSupported) {
        PANIC("The CPU does not support APIC. Required by this kernel");
    }

    // Get the local APIC's base.
    u64 const apicBaseMsr(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE));
    // FIXME: Technically we should mask the bits 63:(MAX_PHY_BITS) here.
    PhyAddr const localApicBase(apicBaseMsr & ~((1 << 12) - 1));
    Log::info("Local APIC base = {}", localApicBase);
    ASSERT(localApicBase.isPageAligned());

    // Use the Direct Map for accessing the LAPIC.
    Paging::PageAttr const attrs(Paging::PageAttr::Writable
                                 | Paging::PageAttr::CacheDisable
                                 | Paging::PageAttr::WriteThrough);
    VirAddr const lapicVirBase(Paging::toVirAddr(localApicBase));
    if (Paging::map(lapicVirBase, localApicBase, attrs, 1)) {
        PANIC("Could not map local APIC to virtual memory");
    }
    LocalApicBase = lapicVirBase;

    Log::debug("Enabling APIC");
    // Enabling the APIC by setting the APIC Global Enable bit in the
    // IA32_APIC_BASE MSR.
    u64 const newApicBaseMsr(apicBaseMsr | (1 << 11));
    Cpu::wrmsr(Cpu::Msr::IA32_APIC_BASE, newApicBaseMsr);
    Log::info("APIC enabled");

    // For now, mask all LVTs. TODO: Do what needs to be done here.
    writeRegister(Register::TimerLocalVectorTableEntry,
        readRegister(Register::TimerLocalVectorTableEntry) | (1 << 16));
    writeRegister(Register::ThermalLocalVectorTableEntry,
        readRegister(Register::ThermalLocalVectorTableEntry) | (1 << 16));
    writeRegister(Register::PerformanceCounterLocalVectorTableEntry,
        readRegister(Register::PerformanceCounterLocalVectorTableEntry)
        | (1 << 16));
    writeRegister(Register::LocalInterrupt0VectorTableEntry,
        readRegister(Register::LocalInterrupt0VectorTableEntry) | (1 << 16));
    writeRegister(Register::LocalInterrupt1VectorTableEntry,
        readRegister(Register::LocalInterrupt1VectorTableEntry) | (1 << 16));
    writeRegister(Register::ErrorVectorTableEntry,
        readRegister(Register::ErrorVectorTableEntry) | (1 << 16));
}

}
}
