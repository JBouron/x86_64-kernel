// Functions and types related to the Advanced Programmable Interrupt Controller
// (APIC).
#pragma once
#include <interrupts/interrupts.hpp>

namespace Interrupts {

class LocalApic {
public:
    // Construct an interface for a Local APIC.
    // @param base: The base of the local APIC registers.
    LocalApic(PhyAddr const base);

    // Mode for the LAPIC timer.
    enum class TimerMode {
        // Timer fires a single interrupt once the timer expires and is not
        // re-armed.
        OneShot = 0,
        // The timer is automatically re-armed after firing an interrupt at
        // expiration time.
        Periodic = 1,
    };

    // Divisor for the timer clock.
    enum class TimerClockDivisor : u8 {
        DivideBy2   = 0b000,
        DivideBy4   = 0b001,
        DivideBy8   = 0b010,
        DivideBy16  = 0b011,
        DivideBy32  = 0b100,
        DivideBy64  = 0b101,
        DivideBy128 = 0b110,
        DivideBy1   = 0b111,
    };

    // Setup the LAPIC timer with the given configuration. This does NOT start
    // the timer.
    // @param mode: The mode to use for the timer.
    // @param vector: The interrupt vector that should be raised when the timer
    // expires.
    void setupTimer(TimerMode const mode, Vector const vector);

    // Start or reset the timer. Must be called after setting up the timer using
    // setupTimer.
    // @param numTicks: The number of timer clock ticks before the timer expires
    // and fires an interrupt.
    // @param div: The divisor to use for the timer clock.
    void resetTimer(u32 const numTicks,
                    TimerClockDivisor const div = TimerClockDivisor::DivideBy1);

    // Stop the timer, the timer won't ever expire until it is reset using
    // resteTimer.
    void stopTimer();

    // Notify the local APIC of an end-of-interrupt.
    void eoi();

    // Run the Local APIC tests.
    static void Test(SelfTests::TestRunner& runner);
    friend SelfTests::TestResult localApicRegisterReadWriteTest();
    friend SelfTests::TestResult localApicSetupTimerTest();
    friend SelfTests::TestResult localApicResetTimerTest();
private:
    // The base address of the registers.
    PhyAddr m_base;

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

    // Check if a local APIC register can be written to, e.g. not read-only.
    // @param reg: The register to check.
    // @return: true if the register is writtable, false otherwise.
    static bool isRegisterWritable(Register const reg);

    // Read a register from the local APIC.
    // @param reg: The register to read from.
    // @return: The current value of the register.
    u32 readRegister(Register const reg) const;

    // Write a register into the local APIC.
    // @param reg: The register to write.
    // @param value: The value to write into the register.
    void writeRegister(Register const reg, u32 const value);

    // Base type for the values read from and written to the LVT registers, e.g.
    // timer, performance, lint, ...
    // This type only defines the bits common between all LVTs. Each LVT
    // register then has its own associated type, e.g. TimerLvt, ... which
    // further defines the bits specific that that LVT.
    // LVT values can be constructed in two ways:
    //   - From a raw u32: This happens when reading an LVT register.
    //   - From a set of configuration parameters: Usually done when this LVT is
    //   meant to be written into an LVT register.
    class Lvt {
    public:
        // The message type. Only used for LINT and Apic Error LVTs.
        enum class MessageType {
            Fixed = 0b000,
            Smi = 0b010,
            Nmi = 0b100,
            External = 0b111,
        };

        // Construct a default LVT, all bits to zero except the mask bit which
        // is set.
        Lvt();

        // Construct an LVT from a vector. The mask bit is unset.
        Lvt(Vector const vector);

        // Construct an LVT from a raw value read from a register.
        Lvt(u32 const raw);

        // Set the value of the mask bit in this TimerLvt.
        // @param isMasked: Indicates if the TimerLvt should be marked as
        // masked, e.g. no interrupt fires at expiration time.
        void setMasked(bool const isMasked);

        // Compute the raw value from this Lvt value. This must be defined by
        // the deriving types.
        virtual u32 raw() const = 0;

    protected:
        bool m_masked;
        Vector m_vector;
    };

    // Type for the TimerLocalVectorTableEntry register.
    class TimerLvt : public Lvt {
    public:
        // Count the Delivery Status (DS) bit as reserved as it is read-only.
        static u64 const ReservedBitsMask = ~((3 << 16) | 0xff);

        // Construct a default LVT, all bits to zero except the mask bit which
        // is set.
        TimerLvt();

        // Construct a TimerLvt with the given configuration.
        // @param timerMode: The timer mode to use.
        // @param vector: The vector to be fired at timer expiration.
        TimerLvt(TimerMode const timerMode, Vector const vector);

        // Construct a TimerLvt from a raw value of an APIC register.
        // @param raw: The raw value to construct the TimerLvt from.
        TimerLvt(u32 const raw);

        // Get the raw value for this TimerLvt. Used when actually writing the
        // TimerLvt into an APIC register.
        virtual u32 raw() const;

    private:
        TimerMode m_timerMode;
    };

    // Type for the LocalInterrupt0VectorTableEntry and
    // LocalInterrupt1VectorTableEntry registers.
    class LintLvt : public Lvt {
    public:
        // The trigger mode for the input pin.
        enum class TriggerMode {
            EdgeTriggered = 0,
            LevelTriggered = 1,
        };

        // Count the Delivery Status (DS) bit as reserved as it is read-only.
        static u64 const ReservedBitsMask = ~((7 << 14) | 0x7ff);

        // Construct a default LVT, all bits to zero except the mask bit which
        // is set.
        LintLvt();

        // Construct a LintLvt. The resulting mask bit is unset.
        // @param triggerMode: The trigger mode of the LINT{0-1}.
        // @param messageType: The message type for the LINT{0-1}.
        // @param vector: The vector to fire when the LINT{0-1} is asserted.
        LintLvt(TriggerMode const triggerMode,
                MessageType const messageType,
                Vector const vector);

        // Construct a LintLvt from a raw value of an APIC register.
        // @param raw: The raw value to construct the LintLvt from.
        LintLvt(u32 const raw);

        // Get the raw value for this LintLVT. Used when actually writing the
        // LintLVT into an APIC register.
        u32 raw() const;

    private:
        TriggerMode m_triggerMode;
        MessageType m_messageType;
    };

    // Type for the ErrorVectorTableEntry LVT.
    class ApicErrorLvt : public Lvt {
    public:
        // Count the Delivery Status (DS) bit as reserved as it is read-only.
        static u64 const ReservedBitsMask = ~((1 << 16) | 0x7ff);

        // Construct a default LVT, all bits to zero except the mask bit which
        // is set.
        ApicErrorLvt();

        // Construct an ApicErrorLvt. The mask bit is unset.
        // @param messageType: The message type to be sent in case of APIC
        // error.
        // @param vector: The vector to fire.
        ApicErrorLvt(MessageType const messageType, Vector const vector);

        // Construct a ApicErrorLvt from a raw value of an APIC register.
        // @param raw: The raw value to construct the ApicErrorLvt from.
        ApicErrorLvt(u32 const raw);

        // Get the raw value for this ApicErrorLvt. Used when actually writing
        // the ApicErrorLvt into an APIC register.
        u32 raw() const;
    private:
        MessageType m_messageType;
    };
    // TODO: Performance Monitor LVT + Thermal LVT if we ever need them.
};

// Initialize the APIC.
void InitLocalApic();

// Notify the local APIC of the End-Of-Interrupt.
void eoi();

}
