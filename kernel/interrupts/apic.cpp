// Functions and types related to the Advanced Programmable Interrupt Controller
// (APIC).
#include "apic.hpp"
#include <interrupts/interrupts.hpp>
#include <paging/paging.hpp>
#include <cpu/cpu.hpp>
#include <util/assert.hpp>

namespace Interrupts {

// Construct an interface for a Local APIC.
// @param base: The base of the local APIC registers.
LocalApic::LocalApic(PhyAddr const base) : m_base(base) {
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
    VirAddr const vaddr(Paging::toVirAddr(base));
    if (Paging::map(vaddr, base, attrs, 1)) {
        PANIC("Could not map local APIC to virtual memory");
    }

    Log::debug("Enabling APIC");
    // Enabling the APIC by setting the APIC Global Enable bit in the
    // IA32_APIC_BASE MSR.
    u64 const apicBaseMsr(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE));
    u64 const newApicBaseMsr(apicBaseMsr | (1 << 11));
    Cpu::wrmsr(Cpu::Msr::IA32_APIC_BASE, newApicBaseMsr);
    Log::info("APIC enabled");
}

// Setup the LAPIC timer with the given configuration. This does NOT start
// the timer.
// @param mode: The mode to use for the timer.
// @param vector: The interrupt vector that should be raised when the timer
// expires.
void LocalApic::setupTimer(TimerMode const mode, Vector const vector) {
    // First stop the timer to avoid any suprise interrupts.
    stopTimer();
    TimerLvt const lvtValue(mode, vector);
    u32 const currLvt(readRegister(Register::TimerLocalVectorTableEntry));
    u32 const newLvt((currLvt & TimerLvt::ReservedBitsMask)
                     | (lvtValue.raw() & ~TimerLvt::ReservedBitsMask));
    writeRegister(Register::TimerLocalVectorTableEntry, newLvt);
}

// Start or reset the timer. Must be called after setting up the timer using
// setupTimer.
// @param numTicks: The number of timer clock ticks before the timer expires
// and fires an interrupt.
// @param div: The divisor to use for the timer clock.
void LocalApic::resetTimer(u32 const numTicks, TimerClockDivisor const div) {
    // Setup the divisor first to avoid race-conditions.
    u32 const divRaw(static_cast<u8>(div));
    u32 const divResv(readRegister(Register::TimerDivideConfiguration) & ~0xb);
    u32 const divValue(divResv | ((divRaw & 0xb100) << 3) | (divRaw & 0b11));
    writeRegister(Register::TimerDivideConfiguration, divValue);
    // Start the timer.
    writeRegister(Register::TimerInitialCount, numTicks);
}

// Stop the timer, the timer won't ever expire until it is reset using
// resteTimer.
void LocalApic::stopTimer() {
    writeRegister(Register::TimerInitialCount, 0x0);
}

// Notify the local APIC of an end-of-interrupt.
void LocalApic::eoi() {
    writeRegister(Register::EndOfInterrupt, 0x0);
}

// Read a register from the local APIC.
// @param reg: The register to read from.
// @return: The current value of the register.
u32 LocalApic::readRegister(Register const reg) const {
    if (reg == Register::EndOfInterrupt) {
        // The EOI register is write-only.
        PANIC("Attempt to read the EOI APIC register, which is write-only");
    }
    VirAddr const regAddr(Paging::toVirAddr(m_base) + static_cast<u16>(reg));
    return *regAddr.ptr<u32>();
}

// Write a register into the local APIC.
// @param reg: The register to write.
// @param value: The value to write into the register.
void LocalApic::writeRegister(Register const reg, u32 const value) {
    VirAddr const regAddr(Paging::toVirAddr(m_base) + static_cast<u16>(reg));
    if (!isRegisterWritable(reg)) {
        PANIC("Attempt to write into read-only APIC register {x}", regAddr);
    }
    *regAddr.ptr<u32>() = value;
}

// Check if a local APIC register can be written to, e.g. not read-only.
// @param reg: The register to check.
// @return: true if the register is writtable, false otherwise.
bool LocalApic::isRegisterWritable(Register const reg) {
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

// Construct a default LVT, all bits to zero except the mask bit which
// is set.
LocalApic::Lvt::Lvt() : m_masked(true), m_vector(0) {}

// Construct an LVT from a vector. The mask bit is unset.
LocalApic::Lvt::Lvt(Vector const vector) : m_masked(false), m_vector(vector) {}

// Construct an LVT from a raw value read from a register.
LocalApic::Lvt::Lvt(u32 const raw) : m_masked(!!(raw & (1 << 16))),
                                m_vector(raw & 0xff) {}

// Set the value of the mask bit in this TimerLvt.
// @param isMasked: Indicates if the TimerLvt should be marked as
// masked, e.g. no interrupt fires at expiration time.
void LocalApic::Lvt::setMasked(bool const isMasked) {
    m_masked = isMasked;
}

// Construct a default LVT, all bits to zero except the mask bit which
// is set.
LocalApic::TimerLvt::TimerLvt() : Lvt(), m_timerMode(TimerMode::OneShot) {}

// Construct a TimerLvt with the given configuration.
// @param timerMode: The timer mode to use.
// @param vector: The vector to be fired at timer expiration.
LocalApic::TimerLvt::TimerLvt(TimerMode const timerMode, Vector const vector) :
    Lvt(vector), m_timerMode(timerMode) {}

// Construct a TimerLvt from a raw value of an APIC register.
// @param raw: The raw value to construct the TimerLvt from.
LocalApic::TimerLvt::TimerLvt(u32 const raw) :
    Lvt(raw), m_timerMode(static_cast<TimerMode>(!!(raw & (1 << 17)))) {}

// Get the raw value for this TimerLvt. Used when actually writing the
// TimerLvt into an APIC register.
u32 LocalApic::TimerLvt::raw() const {
    return (static_cast<u64>(m_timerMode) << 17)
           | (static_cast<u64>(m_masked) << 16)
           | m_vector.raw();
}

// Construct a default LVT, all bits to zero except the mask bit which
// is set.
LocalApic::LintLvt::LintLvt() : Lvt(),
                                m_triggerMode(TriggerMode::EdgeTriggered),
                                m_messageType(Lvt::MessageType::Fixed) {}

// Construct a LintLvt. The resulting mask bit is unset.
// @param triggerMode: The trigger mode of the LINT{0-1}.
// @param messageType: The message type for the LINT{0-1}.
// @param vector: The vector to fire when the LINT{0-1} is asserted.
LocalApic::LintLvt::LintLvt(TriggerMode const triggerMode,
                            MessageType const messageType,
                            Vector const vector) :
    Lvt(vector), m_triggerMode(triggerMode), m_messageType(messageType) {}

// Get the raw value for this LintLVT. Used when actually writing the
// LintLVT into an APIC register.
u32 LocalApic::LintLvt::raw() const {
    return (static_cast<u64>(m_masked) << 16)
           | (static_cast<u64>(m_triggerMode) << 15)
           | (static_cast<u64>(m_messageType) << 8)
           | m_vector.raw();
}

// Construct a LintLvt from a raw value of an APIC register.
// @param raw: The raw value to construct the LintLvt from.
LocalApic::LintLvt::LintLvt(u32 const raw) :
    Lvt(raw), m_triggerMode(static_cast<TriggerMode>(!!(raw & (1 << 15)))),
    m_messageType(static_cast<MessageType>((raw >> 8) & 0x7)) {}

// Construct a default LVT, all bits to zero except the mask bit which
// is set.
LocalApic::ApicErrorLvt::ApicErrorLvt() : Lvt(),
    m_messageType(Lvt::MessageType::Fixed) {}

// Construct an ApicErrorLvt. The mask bit is unset.
// @param messageType: The message type to be sent in case of APIC
// error.
// @param vector: The vector to fire.
LocalApic::ApicErrorLvt::ApicErrorLvt(MessageType const messageType,
                                      Vector const vector) :
    Lvt(vector), m_messageType(messageType) {}

// Construct a ApicErrorLvt from a raw value of an APIC register.
// @param raw: The raw value to construct the ApicErrorLvt from.
LocalApic::ApicErrorLvt::ApicErrorLvt(u32 const raw) :
    Lvt(raw), m_messageType(static_cast<MessageType>((raw >> 8) & 0x7)) {}

// Get the raw value for this ApicErrorLvt. Used when actually writing
// the ApicErrorLvt into an APIC register.
u32 LocalApic::ApicErrorLvt::raw() const {
    return (static_cast<u64>(m_masked) << 16)
           | (static_cast<u64>(m_messageType) << 8)
           | m_vector.raw();
}

// Pointer to the global instance of LocalApic.
// TODO: Once we have multi-cores this should be per CPU.
static LocalApic* LOCAL_APIC = nullptr;

// Initialize the APIC.
void InitLocalApic() {
    ASSERT(!LOCAL_APIC);
    // Get the local APIC's base.
    u64 const apicBaseMsr(Cpu::rdmsr(Cpu::Msr::IA32_APIC_BASE));
    // FIXME: Technically we should mask the bits 63:(MAX_PHY_BITS) here.
    PhyAddr const localApicBase(apicBaseMsr & ~((1 << 12) - 1));
    Log::info("Local APIC base = {}", localApicBase);
    ASSERT(localApicBase.isPageAligned());

    static LocalApic instance(localApicBase);
    LOCAL_APIC = &instance;
}

// Notify the local APIC of the End-Of-Interrupt.
void eoi() {
    ASSERT(!!LOCAL_APIC);
    LOCAL_APIC->eoi();
}
}
