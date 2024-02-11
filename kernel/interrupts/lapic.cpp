// Functions and types related to the local Advanced Programmable Interrupt
// Controller (LAPIC).
#include <interrupts/lapic.hpp>
#include <util/assert.hpp>
#include <util/panic.hpp>
#include <paging/paging.hpp>
#include <smp/smp.hpp>
#include <datastruct/vector.hpp>

namespace Interrupts {

// Construct an interface for a Local APIC.
// @param base: The base of the local APIC registers.
Lapic::Lapic(PhyAddr const base) : m_base(base) {
    // Check that the CPU supports APIC. Virtually all CPUs do.
    bool const isApicSupported(Cpu::cpuid(0x1).edx & (1 << 9));
    if (!isApicSupported) {
        PANIC("The CPU does not support APIC. Required by this kernel");
    }

    // Remap the virtual address in the Direct Map, associated with the base
    // with CacheDisable and WriteThrough attributes.
    Paging::PageAttr const attrs(Paging::PageAttr::Writable
                                 | Paging::PageAttr::CacheDisable
                                 | Paging::PageAttr::WriteThrough);
    VirAddr const vaddr(base.toVir());
    if (Paging::map(vaddr, base, attrs, 1)) {
        PANIC("Could not map local APIC to virtual memory");
    }

    Log::debug("Enabling APIC on cpu {}", Smp::id());
    // Enabling the APIC by setting the APIC Global Enable bit in the
    // IA32_APIC_BASE MSR.
    u64 const apicBaseMsr(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE));
    u64 const newApicBaseMsr(apicBaseMsr | (1 << 11));
    Cpu::wrmsr(Cpu::Msr::IA32_APIC_BASE, newApicBaseMsr);
    Log::info("APIC enabled on cpu {}", Smp::id());
}

// Extract the value in bits hiBit ... lowBit from `value`.
static inline u32 bits(u32 const value, u8 const hiBit, u8 const lowBit) {
    ASSERT(lowBit <= hiBit);
    u32 const width(hiBit - lowBit + 1);
    return (value >> lowBit) & ((1 << width) - 1);
}

// Get the APIC ID of this local APIC. Each core has a unique apic ID which
// value is determined by hardware.
Lapic::Id Lapic::apicId() const {
    return bits(readRegister(Register::ApicId), 31, 24);
}

// Get the version information from the local APIC.
Lapic::VersionInfo Lapic::version() const {
    u32 const raw(readRegister(Register::ApicVersion));
    VersionInfo const verInfo({
        .version = u8(bits(raw, 7, 0)),
        .maxLvtEntries = u8(bits(raw, 23, 16)),
        .hasExtendedApicRegisters = !!bits(raw, 31, 31)
    });
    return verInfo;
}

// Compute the raw value to be written to an APIC register.
// @return: The raw value corresponding to this PriorityInfo.
u32 Lapic::PriorityInfo::raw() const {
    return (priority.raw() << 4) | (prioritySubClass.raw());
}

// Get the current Task Priority for this core.
Lapic::PriorityInfo Lapic::taskPriority() const {
    u32 const raw(readRegister(Register::TaskPriority));
    PriorityInfo const prio({
        .prioritySubClass = PriorityInfo::PrioritySubClass(bits(raw, 3, 0)),
        .priority = PriorityInfo::Priority(bits(raw, 7, 4)),
    });
    return prio;
}
// Set the Task Priority for this core.
void Lapic::setTaskPriority(PriorityInfo const& taskPrio) {
    writeRegister(Register::TaskPriority,
                  taskPrio.raw(),
                  WriteMask::TaskPriority);
}

// Get the current Arbitration Priority for this core.
Lapic::PriorityInfo Lapic::arbitrationPriority() const {
    u32 const raw(readRegister(Register::ArbitrationPriority));
    PriorityInfo const prio({
        .prioritySubClass = PriorityInfo::PrioritySubClass(bits(raw, 3, 0)),
        .priority = PriorityInfo::Priority(bits(raw, 7, 4)),
    });
    return prio;
}

// Get the current Processor Priority for this core.
Lapic::PriorityInfo Lapic::processorPriority() const {
    u32 const raw(readRegister(Register::ProcessorPriority));
    PriorityInfo const prio({
        .prioritySubClass = PriorityInfo::PrioritySubClass(bits(raw, 3, 0)),
        .priority = PriorityInfo::Priority(bits(raw, 7, 4)),
    });
    return prio;
}

void Lapic::endOfInterrupt() {
    writeRegister(Register::EndOfInterrupt, 0, WriteMask::EndOfInterrupt);
}

// Get the value from a read to a remote APIC. Only valid after issuing a
// remote read request using the Interrupt Command Register and if the
// readRemoteStatus of the ICR is set to DataAvailable.
u32 Lapic::remoteRead() const {
    return readRegister(Register::RemoteRead);
}

// Get the Destination Logical ID of this local APIC.
Lapic::DestLogicalId Lapic::logicalDestination() const {
    return bits(readRegister(Register::LogicalDestination), 31, 24);
}

// Set the Destination Logical ID of this local APIC. Used for logical
// destination modes.
void Lapic::setLogicalDestination(DestLogicalId const dlid) {
    u32 const val(u32(static_cast<u8>(dlid)) << 24);
    writeRegister(Register::LogicalDestination,
                  val,
                  WriteMask::LogicalDestination);
}

// Get the current model of logica destinations.
Lapic::DestFmtModel Lapic::destinationFormat() const {
    u32 const raw(bits(readRegister(Register::DestinationFormat), 31, 28));
    ASSERT(raw == 0 || raw == 0xf);
    return static_cast<DestFmtModel>(raw);
}

// Set the model for the logical destinations.
void Lapic::setDestinationFormat(DestFmtModel const model) {
    u32 const raw(u32(static_cast<u8>(model)) << 28);
    writeRegister(Register::DestinationFormat,
                  raw,
                  WriteMask::DestinationFormat);
}

// Compute the raw value to be written to an APIC register.
// @return: The raw value corresponding to this SpuriousInterrupt.
u32 Lapic::SpuriousInterrupt::raw() const {
    return (u32(focusCpuCoreScheduling) << 9)
           | (u32(apicSoftwareEnable) << 8)
           | vector.raw();
}

// Get the current value of the Spurious Interrupt Register.
Lapic::SpuriousInterrupt Lapic::spuriousInterrupt() const {
    u32 const raw(readRegister(Register::SpuriousInterruptVector));
    SpuriousInterrupt const spur({
        .vector = Vector(bits(raw, 7, 0)),
        .apicSoftwareEnable = !!bits(raw, 8, 8),
        .focusCpuCoreScheduling = !!bits(raw, 9, 9),
    });
    return spur;
}

// Set the value of the Spurious Interrupt Register.
void Lapic::setSpuriousInterrupt(SpuriousInterrupt const& spuriousInt) {
    writeRegister(Register::SpuriousInterruptVector,
                  spuriousInt.raw(),
                  WriteMask::SpuriousInterruptVector);
}

// Get the current value of the In-Service-Register.
Lapic::Bitmap Lapic::inService() const {
    Bitmap map;
    for (u64 i(0); i < 8; ++i) {
        Register const reg(static_cast<Register>(
            static_cast<u16>(Register::InService) + i * 0x10));
        map.dword[i] = readRegister(reg);
    }
    return map;
}

// Get the current value of the Trigger-Mode-Register.
Lapic::Bitmap Lapic::triggerMode() const {
    Bitmap map;
    for (u64 i(0); i < 8; ++i) {
        Register const reg(static_cast<Register>(
            static_cast<u16>(Register::TriggerMode) + i * 0x10));
        map.dword[i] = readRegister(reg);
    }
    return map;
}

// Get the current value of the Interrupt-Request-Register.
Lapic::Bitmap Lapic::interruptRequest() const {
    Bitmap map;
    for (u64 i(0); i < 8; ++i) {
        Register const reg(static_cast<Register>(
            static_cast<u16>(Register::InterruptRequest) + i * 0x10));
        map.dword[i] = readRegister(reg);
    }
    return map;
}

// Compute the raw value to be written to an APIC register.
// @return: The raw value corresponding to this ErrorStatus.
u32 Lapic::ErrorStatus::raw() const {
    return (u32(sentAcceptError) << 2)
           | (u32(receiveAcceptError) << 3)
           | (u32(sentIllegalVector) << 5)
           | (u32(receivedIllegalVector) << 6)
           | (u32(illegalRegsiterAddress) << 7);
}

// Get the ErrorStatus for the errors that occured since the last write to
// the Error Status Register.
Lapic::ErrorStatus Lapic::errorStatus() const {
    u32 const raw(readRegister(Register::ErrorStatus));
    ErrorStatus const status({
        .sentAcceptError = !!bits(raw, 2, 2),
        .receiveAcceptError = !!bits(raw, 3, 3),
        .sentIllegalVector = !!bits(raw, 5, 5),
        .receivedIllegalVector = !!bits(raw, 6, 6),
        .illegalRegsiterAddress = !!bits(raw, 7, 7),
    });
    return status;
}

// Write the ErrorStatus register.
// @param errStatus: The value to write.
void Lapic::setErrorStatus(ErrorStatus const& errStatus) {
    writeRegister(Register::ErrorStatus,
                  errStatus.raw(),
                  WriteMask::ErrorStatus);
}

// Compute the raw value to be written to an APIC register.
// @return: The raw value corresponding to this InterruptCmd.
u64 Lapic::InterruptCmd::raw() const {
    return (u64(destination) << 56)
           | (u64(static_cast<u8>(destinationShorthand)) << 18)
           | (u64(static_cast<u8>(readRemoteStatus)) << 16)
           | (u64(static_cast<u8>(triggerMode)) << 15)
           | (u64(static_cast<u8>(level)) << 14)
           | (u64(static_cast<u8>(deliveryStatus)) << 12)
           | (u64(static_cast<u8>(destinationMode)) << 11)
           | (u64(static_cast<u8>(messageType)) << 8)
           | vector.raw();
}

// Check if this Lvt is a valid configuration.
// @return: true if is is valid, false otherwise.
bool Lapic::InterruptCmd::isValid() const {
    if (messageType == InterruptCmd::MessageType::Smi
        && (vector != 0 || triggerMode != TriggerMode::EdgeTriggered)) {
        return false;
    } else if (messageType == InterruptCmd::MessageType::Init
        && (vector != 0 || triggerMode != TriggerMode::EdgeTriggered)) {
        return false;
    } else {
        return true;
    }
}

Lapic::InterruptCmd Lapic::interruptCommand() const {
    Register const highReg(static_cast<Register>(
        static_cast<u16>(Register::InterruptCommand) + 0x10));
    u32 const rawHigh(readRegister(highReg));
    u32 const rawLow(readRegister(Register::InterruptCommand));
    InterruptCmd const icr({
        .vector = Vector(bits(rawLow, 7, 0)),
        .messageType =
            static_cast<InterruptCmd::MessageType>(bits(rawLow, 10, 8)),
        .destinationMode =
            static_cast<InterruptCmd::DestinationMode>(bits(rawLow, 11, 11)),
        .deliveryStatus = !!bits(rawLow, 12, 12),
        .level = !!bits(rawLow, 14, 14),
        .triggerMode = static_cast<TriggerMode>(bits(rawLow, 15, 15)),
        .readRemoteStatus =
            static_cast<InterruptCmd::ReadRemoteStatus>(bits(rawLow, 17, 16)),
        .destinationShorthand = static_cast<InterruptCmd::DestinationShorthand>(
            bits(rawLow, 19, 18)),
        .destination = bits(rawHigh, 63, 56)
    });
    return icr;
}

void Lapic::setInterruptCommand(InterruptCmd const& icr) {
    // Writing the low order DWORD of the ICR causes the IPI to be sent, hence
    // write the high order DWORD first.
    u64 const raw(icr.raw());
    u32 const high(raw >> 32);
    u32 const low(raw & 0xffffffff);
    Register const highReg(static_cast<Register>(
        static_cast<u16>(Register::InterruptCommand) + 0x10));
    writeRegister(highReg, high, WriteMask::InterruptCommandHigh);
    writeRegister(Register::InterruptCommand,
                  low,
                  WriteMask::InterruptCommandLow);
}

// General Local Vector Table register format. Note that depending on which
// register this value is written, not all fields/bits might be written.

// Compute the raw value to be written to an APIC register.
// @return: The raw value corresponding to this Lvt.
u32 Lapic::Lvt::raw() const {
    return (u32(static_cast<u8>(timerMode)) << 17)
           | (u32(mask) << 16)
           | (u32(static_cast<u8>(triggerMode)) << 15)
           | (u32(static_cast<u8>(remoteIRR)) << 14)
           | (u32(static_cast<u8>(deliveryStatus)) << 12)
           | (u32(static_cast<u8>(messageType)) << 8)
           | vector.raw();
}

// Check if this Lvt is a valid configuration.
// @return: true if is is valid, false otherwise.
bool Lapic::Lvt::isValid() const {
    if (messageType == Lvt::MessageType::Smi
        && (vector != 0 || triggerMode != TriggerMode::EdgeTriggered)) {
        return false;
    } else {
        return true;
    }
}

// Get the current value of the Timer LVT.
Lapic::Lvt Lapic::timerLvt() const {
    u32 const raw(readRegister(Register::TimerLocalVectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .remoteIRR = !!bits(raw, 14, 14),
        .triggerMode = static_cast<TriggerMode>(bits(raw, 15, 15)),
        .mask = !!bits(raw, 16, 16),
        .timerMode = static_cast<Lvt::TimerMode>(bits(raw, 17, 17)),
    });
    return lvt;
}

// Set the current value of the Timer LVT.
void Lapic::setTimerLvt(Lvt const& lvt) {
    writeRegister(Register::TimerLocalVectorTableEntry,
                  lvt.raw(),
                  WriteMask::TimerLocalVectorTableEntry);
}

// Get the current value of the Thermal LVT.
Lapic::Lvt Lapic::thermalLvt() const {
    u32 const raw(readRegister(Register::ThermalLocalVectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .mask = !!bits(raw, 16, 16),
    });
    return lvt;
}
// Set the current value of the Thermal LVT.
void Lapic::setThermalLvt(Lvt const& lvt) {
    writeRegister(Register::ThermalLocalVectorTableEntry,
                  lvt.raw(),
                  WriteMask::ThermalLocalVectorTableEntry);
}

// Get the current value of the Performance Counter LVT.
Lapic::Lvt Lapic::performanceCounterLvt() const {
    u32 const raw(readRegister(
        Register::PerformanceCounterLocalVectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .mask = !!bits(raw, 16, 16),
    });
    return lvt;
}
// Set the current value of the Performance Counter LVT.
void Lapic::setPerformanceCounterLvt(Lvt const& lvt) {
    writeRegister(Register::PerformanceCounterLocalVectorTableEntry,
                  lvt.raw(),
                  WriteMask::PerformanceCounterLocalVectorTableEntry);
}

// Get the current value of the LINT0 LVT.
Lapic::Lvt Lapic::localInterrupt0Lvt() const {
    u32 const raw(readRegister(Register::LocalInterrupt0VectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .remoteIRR = !!bits(raw, 14, 14),
        .triggerMode = static_cast<TriggerMode>(bits(raw, 15, 15)),
        .mask = !!bits(raw, 16, 16),
    });
    return lvt;
}
// Set the current value of the LINT0 LVT.
void Lapic::setLocalInterrupt0Lvt(Lvt const& lvt) {
    writeRegister(Register::LocalInterrupt0VectorTableEntry,
                  lvt.raw(),
                  WriteMask::LocalInterrupt0VectorTableEntry);
}
// Get the current value of the LINT1 LVT.
Lapic::Lvt Lapic::localInterrupt1Lvt() const {
    u32 const raw(readRegister(Register::LocalInterrupt1VectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .remoteIRR = !!bits(raw, 14, 14),
        .triggerMode = static_cast<TriggerMode>(bits(raw, 15, 15)),
        .mask = !!bits(raw, 16, 16),
    });
    return lvt;
}
// Set the current value of the LINT1 LVT.
void Lapic::setLocalInterrupt1Lvt(Lvt const& lvt) {
    writeRegister(Register::LocalInterrupt1VectorTableEntry,
                  lvt.raw(),
                  WriteMask::LocalInterrupt1VectorTableEntry);
}

// Get the current value of the Apic Error LVT.
Lapic::Lvt Lapic::errorLvt() const {
    u32 const raw(readRegister(Register::ErrorVectorTableEntry));
    Lvt const lvt({
        .vector = Vector(bits(raw, 7, 0)),
        .messageType = static_cast<Lvt::MessageType>(bits(raw, 10, 8)),
        .deliveryStatus = !!bits(raw, 12, 12),
        .mask = !!bits(raw, 16, 16),
    });
    return lvt;
}
// Set the current value of the Apic Error LVT.
void Lapic::setErrorLvt(Lvt const& lvt) {
    writeRegister(Register::ErrorVectorTableEntry,
                  lvt.raw(),
                  WriteMask::ErrorVectorTableEntry);
}

// Get the timer initial count.
u32 Lapic::timerInitialCount() const {
    return readRegister(Register::TimerInitialCount);
}
// Set the timer initial count.
void Lapic::setTimerInitialCount(u32 const count) {
    return writeRegister(Register::TimerInitialCount,
                         count,
                         WriteMask::TimerInitialCount);
}

// Get the timer current count.
u32 Lapic::timerCurrentCount() const {
    return readRegister(Register::TimerCurrentCount);
}

// Get the timer current divide configuration.
Lapic::TimerDivideConfiguration Lapic::timerDivideConfiguration() const {
    u32 const raw(readRegister(Register::TimerDivideConfiguration));
    return static_cast<TimerDivideConfiguration>(raw);
}
// Set the timer current divide configuration.
void Lapic::setTimerDivideConfiguration(TimerDivideConfiguration const cfg) {
    writeRegister(Register::TimerDivideConfiguration,
                  static_cast<u8>(cfg),
                  WriteMask::TimerDivideConfiguration);
}

// Read a register from the local APIC.
// @param reg: The register to read from.
// @return: The current value of the register.
u32 Lapic::readRegister(Register const reg) const {
    VirAddr const regAddr(m_base.toVir() + static_cast<u16>(reg));
    return *regAddr.ptr<u32 volatile>();
}

// Write a register into the local APIC.
// @param reg: The register to write.
// @param value: The value to write into the register.
// @param mask: Indicates which bits of the value should be written to the
// register.
void Lapic::writeRegister(Register const reg,
                          u32 const value,
                          WriteMask const mask) {
    VirAddr const regAddr(m_base.toVir() + static_cast<u16>(reg));
    u32 volatile * const ptr(regAddr.ptr<u32 volatile>());
    u32 const rawMask(static_cast<u32>(mask));
    u32 const currValue(*ptr);
    // Technically we don't really need to read the reserved bits since those
    // are supposed to always be zero. But do it anyways for good measure.
    u32 const newValue((currValue & ~rawMask) | (value & rawMask));
    *ptr = newValue;
}

// Each cpu's local apic interface. Although this could be put into the
// PerCpu::Data struct, it does not make much sense to expose this interface to
// other cpus as a cpu can only interact with its own LAPIC. Hence keep it
// hidden here, the only way to access the Lapic interface of a cpu is by
// calling lapic().
static ::Vector<Lapic*> LocalApics;

// Initialize all the Lapic interfaces for all cpus in the system.
void InitLapic() {
    for (u8 i(0); i < Smp::ncpus(); ++i) {
        LocalApics.pushBack(nullptr);
    }
}

// Get a reference to the Local APIC of this cpu.
// @return: Reference to the local APIC of this cpu.
Lapic& lapic() {
    Smp::Id const id(Smp::id());
	if (!LocalApics[id.raw()]) {
        Log::info("Initializing local APIC on cpu {}", id);
        // Get the local APIC's base.
        u64 const apicBaseMsr(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE));
        // FIXME: Technically we should mask the bits 63:(MAX_PHY_BITS) here.
        PhyAddr const localApicBase(apicBaseMsr & ~((1 << 12) - 1));
        Log::info("Local APIC base = {}", localApicBase);
        ASSERT(localApicBase.isPageAligned());

        LocalApics[id.raw()] = new Lapic(localApicBase);
        Log::info("Local APIC initialization on cpu {} done", id);
    }
    return *LocalApics[id.raw()];
}
}
