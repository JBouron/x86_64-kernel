// Functions and types related to the local Advanced Programmable Interrupt
// Controller (LAPIC).
#pragma once
#include <interrupts/interrupts.hpp>
#include <selftests/selftests.hpp>

namespace Interrupts {

// Interface to interact with the local APIC. Having all functions in a class
// makes it easier to test the interface.
class Lapic {
public:
    // Construct an interface for a Local APIC.
    // @param base: The base of the local APIC registers.
    Lapic(PhyAddr const base);

    // Type of the APIC ID. The actual size in bits of an APIC ID depends on
    // whether or not x2APIC is used. Use the max size so that it works
    // regardless of x2APIC being used or not.
    using Id = u32;

    // Get the APIC ID of this local APIC. Each core has a unique apic ID which
    // value is determined by hardware.
    Id apicId() const;

    // Local APIC version information.
    struct VersionInfo {
        // Version number of the APIC implementation.
        u8 version;
        // Maximum number of entries in the Local Vector Table MINUS ONE.
        u8 maxLvtEntries;
        // Indicates the presence of extended APIC registers.
        bool hasExtendedApicRegisters;
    };
    // Get the version information from the local APIC.
    VersionInfo version() const;

    // Interrupt handling priority of a core.
    struct PriorityInfo {
        class Priority : public SubRange<Priority, 0, 0xf> {};
        class PrioritySubClass : public SubRange<PrioritySubClass, 0, 0xf> {};

        PrioritySubClass prioritySubClass;
        Priority priority;

        // Comparison operator.
        bool operator==(PriorityInfo const& other) const = default;

        // Compute the raw value to be written to an APIC register.
        // @return: The raw value corresponding to this PriorityInfo.
        u32 raw() const;
    };

    // Get the current Task Priority for this core.
    PriorityInfo taskPriority() const;
    // Set the Task Priority for this core.
    void setTaskPriority(PriorityInfo const& taskPrio);

    // Get the current Arbitration Priority for this core.
    PriorityInfo arbitrationPriority() const;

    // Get the current Processor Priority for this core.
    PriorityInfo processorPriority() const;

    // Notify the local APIC of End Of Interrupt.
    void endOfInterrupt();

    // Get the value from a read to a remote APIC. Only valid after issuing a
    // remote read request using the Interrupt Command Register and if the
    // readRemoteStatus of the ICR is set to DataAvailable.
    // FIXME: This seems to only be available on AMD cpus? If that is the case
    // we need a check here.
    u32 remoteRead() const;

    using DestLogicalId = u8;
    // Get the Destination Logical ID of this local APIC.
    DestLogicalId logicalDestination() const;
    // Set the Destination Logical ID of this local APIC. Used for logical
    // destination modes.
    void setLogicalDestination(DestLogicalId const dlid);

    // Control the model of the logical destination mode.
    enum class DestFmtModel : u8 {
        // Split logical destinations into two: a cluster ID and a 4-bit bitmap,
        // as follows: <clusterID>:<bitmap>. For an APIC to be a destination,
        // the cluster ID must match and the bitwise AND of the bitmap with the
        // bottom 4-bits of the destination must be non-zero.
        Cluster = 0x0,
        // Logical destinations are 8-bit bitmaps, each bit indicating if the
        // associated local APIC is part of the destination. If 0xff then all
        // APIC(s) receive the message.
        // In this mode the Logical Destination Register of all cores should
        // only have one set bit.
        Flat = 0xf,
    };
    // Get the current model of logica destinations.
    DestFmtModel destinationFormat() const;
    // Set the model for the logical destinations.
    void setDestinationFormat(DestFmtModel const model);

    // Configuration for the Spurious Interrupt Register.
    struct SpuriousInterrupt {
        // The vector that is sent to the CPU in case of a spurious interrupt.
        Vector vector;
        // When cleared, disable the local APIC temporarily. When set enables
        // the local APIC.
        bool apicSoftwareEnable;
        // I don't understand this, see the docs.
        bool focusCpuCoreScheduling;

        // Comparison operator.
        bool operator==(SpuriousInterrupt const& other) const = default;

        // Compute the raw value to be written to an APIC register.
        // @return: The raw value corresponding to this SpuriousInterrupt.
        u32 raw() const;
    };

    // Get the current value of the Spurious Interrupt Register.
    SpuriousInterrupt spuriousInterrupt() const;

    // Set the value of the Spurious Interrupt Register.
    void setSpuriousInterrupt(SpuriousInterrupt const& spuriousInt);

    // 256-bit bitmap.
    struct Bitmap {
        u32 dword[8];
    };

    // Get the current value of the In-Service-Register.
    Bitmap inService() const;
    // Get the current value of the Trigger-Mode-Register.
    Bitmap triggerMode() const;
    // Get the current value of the Interrupt-Request-Register.
    Bitmap interruptRequest() const;

    // Information on errors that are detected while the local APIC is handling
    // interrupts.
    struct ErrorStatus {
        // A message sent by the local APIC was not accepted by any other APIC.
        bool sentAcceptError;
        // A message received by the local APIC was not accepted by this or any
        // other APIC.
        bool receiveAcceptError;
        // The local APIC attempted to send a message with an illegal vector
        // value.
        bool sentIllegalVector;
        // The local APIC received a message with an illegal vector value.
        bool receivedIllegalVector;
        // Indicates an access to an unimplemented register within the local
        // APIC.
        bool illegalRegsiterAddress;

        // Comparison operator.
        bool operator==(ErrorStatus const& other) const = default;

        // Compute the raw value to be written to an APIC register.
        // @return: The raw value corresponding to this ErrorStatus.
        u32 raw() const;
    };
    // Get the ErrorStatus for the errors that occured since the last write to
    // the Error Status Register.
    ErrorStatus errorStatus() const;

    // Write the ErrorStatus register.
    // @param errStatus: The value to write.
    void setErrorStatus(ErrorStatus const& errStatus);

    enum class TriggerMode : u8 {
        EdgeTriggered = 0,
        LevelTriggered = 1,
    };

    // Type for the value of the Interrupt Command Register. Used to send
    // Inter-Processor-Interrupts.
    // FIXME: Some combinations of fields are invalid.
    struct InterruptCmd {
        // Message type of an IPI.
        enum class MessageType : u8 {
            // The IPI delivers an interrupt to the target local APIC specified
            // in the destination field.
            Fixed = 0b000,
            // Deliver the IPI to the APIC executing at the lowest priority of
            // all APIC(s) that match the destination logical ID specified in
            // the destination field.
            LowestPriority = 0b001,
            // The IPI delivers an SMI interrupt at the target APIC(s). Trigger
            // mode is EdgeTriggered and the Vector must be 0.
            Smi = 0b010,
            // The IPI delivers a request to read an APIC register in the target
            // APIC. The trigger mode is edge triggered and the vector field
            // indicates the offset of the register to be read.
            // The readRemoteStatus indicates the completion of the read and the
            // Remote Read Register contains the value read, if successful.
            RemoteRead = 0b011,
            // The IPI delivers an NMI to the target APIC in the destination
            // field. The vector is ignored.
            Nmi = 0b100,
            // The IPI delivers an INIT request to the target APIC(s). Trigger
            // mode is EdgeTriggered and the vector field must be 0.
            Init = 0b101,
            // The IPI delivers a start-up request (SIPI) to the target APIC(s).
            // The target cores start executing at the address specified in the
            // vector field.
            Startup = 0b110,
            // The IPI delivers an external interrupt to the target APIC(s). The
            // interrupt can be delivered event if the target APIC is disabled.
            External = 0b111,
        };

        // Choose between physical and logical mode for the destination of an
        // IPI.
        enum class DestinationMode : u8 {
            Physical = 0,
            Logical = 1,
        };

        // Indicates thet status of a remote read operation.
        enum class ReadRemoteStatus : u8 {
            InvalidRead = 0b00,
            DeliveryPending = 0b01,
            DataAvailable = 0b10,
        };

        // Provide quick shorthand to specify the destination of an IPI.
        enum class DestinationShorthand : u8 {
            // Use the destination field of the ICR as-is.
            DestinationField = 0b00,
            // Send the IPI to ourselves.
            Self = 0b01,
            // Send the IPI to all cores including ourselves.
            AllIncludingSelf = 0b10,
            // Send the IPI to all cores excluding ourselves.
            AllExcludingSelf = 0b11,
        };

        // Varies depending on the message type. For Fixed and LowestPriority
        // this field contains the vector that is sent for this IPI. Ignored for
        // other message types.
        Vector vector;
        // The message type of the interrupt sent to the destination cpu.
        MessageType messageType = MessageType::Fixed;
        // Control how the destination core(s) are computed from the destination
        // field.
        DestinationMode destinationMode = DestinationMode::Physical;
        // (Read-only) Set when the local APIC has sent the IPI and is waiting
        // for it to be accepted by the destination. It is not needed to wait on
        // this bit to send a new IPI; IPIs are always delivered regardless of
        // the value of this bit.
        bool deliveryStatus = false;
        // Don't understand, read the docs.
        bool level = false;
        // Specifies how the IPIs to the local APIC are triggered.
        TriggerMode triggerMode = TriggerMode::EdgeTriggered;
        // The status of a remote read from another local APIC. If DataAvailable
        // the data can be read from the Remote Read Register.
        ReadRemoteStatus readRemoteStatus = ReadRemoteStatus::InvalidRead;
        // Shorthand notation for the destination of the IPI.
        DestinationShorthand destinationShorthand =
            DestinationShorthand::DestinationField;
        // The destination of this IPI, this can either be a single or multiple
        // APIC(s). Ignored if the DestinationShorthand is not DestinationField.
        // This field is interpreted as a physical or logical destination
        // depending on the destination mode.
        u32 destination;

        // Comparison operator.
        bool operator==(InterruptCmd const& other) const = default;

        // Compute the raw value to be written to an APIC register.
        // @return: The raw value corresponding to this InterruptCmd.
        u64 raw() const;

        // Check if this Lvt is a valid configuration.
        // @return: true if is is valid, false otherwise.
        bool isValid() const;
    };
    InterruptCmd interruptCommand() const;
    void setInterruptCommand(InterruptCmd const& icr);

    // General Local Vector Table register format. Note that depending on which
    // register this value is written, not all fields/bits might be written.
    // Some bits are read-only, their values are ignored when writing into an
    // LVT register.
    struct Lvt {
        // Available message type for an LVT.
        enum class MessageType : u8 {
            Fixed = 0b000,
            Smi = 0b010,
            Nmi = 0b100,
            External = 0b111,
        };

        // Mode for the LAPIC timer.
        enum class TimerMode : u8 {
            // Timer fires a single interrupt once the timer expires and is not
            // re-armed.
            OneShot = 0,
            // The timer is automatically re-armed after firing an interrupt at
            // expiration time.
            Periodic = 1,
        };

        // The vector that is sent for this interrupt source when the message
        // type is Fixed. It is ignored when the message type is NMI or SMI.
        // Valid values range from 16 to 255.
        // FIXME: Use custom type to enfore the range.
        Vector vector = Vector(0);
        // The delivery mode sent to the cpu interrupt handler for this source.
        MessageType messageType = MessageType::Fixed;
        // (Read-only) Set when the interrupt is pending at the interrupt
        // handler. Cleared when the interrupt was successfully delivered.
        bool deliveryStatus = false;
        // (Read-only) Set when the local APIC accepts a LINT0 or LINT1
        // interrupt with a LevelTriggered trigger mode. Cleared when the
        // interrupt completes and EOI is received.
        bool remoteIRR = false;
        // How interrupts to the local APIC are triggered. Ignored when the
        // message type is NMI or SMI.
        TriggerMode triggerMode = TriggerMode::EdgeTriggered;
        // If set the interrupt is masked.
        bool mask = false;
        // The mode for the APIC timer. Only used when writing into the timer
        // LVT, ignored otherwise.
        TimerMode timerMode = TimerMode::OneShot;

        // Comparison operator.
        bool operator==(Lvt const& other) const = default;

        // Compute the raw value to be written to an APIC register.
        // @return: The raw value corresponding to this Lvt.
        u32 raw() const;

        // Check if this Lvt is a valid configuration.
        // @return: true if is is valid, false otherwise.
        bool isValid() const;
    };

    // Get the current value of the Timer LVT.
    Lvt timerLvt() const;
    // Set the current value of the Timer LVT.
    void setTimerLvt(Lvt const& lvt);

    // Get the current value of the Thermal LVT.
    Lvt thermalLvt() const;
    // Set the current value of the Thermal LVT.
    void setThermalLvt(Lvt const& lvt);

    // Get the current value of the Performance Counter LVT.
    Lvt performanceCounterLvt() const;
    // Set the current value of the Performance Counter LVT.
    void setPerformanceCounterLvt(Lvt const& lvt);

    // Get the current value of the LINT0 LVT.
    Lvt localInterrupt0Lvt() const;
    // Set the current value of the LINT0 LVT.
    void setLocalInterrupt0Lvt(Lvt const& lvt);
    // Get the current value of the LINT1 LVT.
    Lvt localInterrupt1Lvt() const;
    // Set the current value of the LINT1 LVT.
    void setLocalInterrupt1Lvt(Lvt const& lvt);

    // Get the current value of the Apic Error LVT.
    Lvt errorLvt() const;
    // Set the current value of the Apic Error LVT.
    void setErrorLvt(Lvt const& lvt);

    // Get the timer initial count.
    u32 timerInitialCount() const;
    // Set the timer initial count.
    void setTimerInitialCount(u32 const count);

    // Get the timer current count.
    u32 timerCurrentCount() const;

    // Control the clock divide for the local APIC timer.
    enum class TimerDivideConfiguration : u8 {
        DivideBy2   = 0b0000,
        DivideBy4   = 0b0001,
        DivideBy8   = 0b0010,
        DivideBy16  = 0b0011,
        DivideBy32  = 0b1000,
        DivideBy64  = 0b1001,
        DivideBy128 = 0b1010,
        DivideBy1   = 0b1011,
    };

    // Get the timer current divide configuration.
    TimerDivideConfiguration timerDivideConfiguration() const;
    // Set the timer current divide configuration.
    void setTimerDivideConfiguration(TimerDivideConfiguration const cfg);

    // Run the Lapic tests.
    static void Test(SelfTests::TestRunner& runner);
    friend SelfTests::TestResult lapicConstantsTest();
    friend SelfTests::TestResult lapicReadTest();
private:
    // All APIC registers. For registers that span more than one DWORD (e.g IRR,
    // ISR, TMR and ICR) need to static_cast to u16 in order to access to the
    // subsequent DWORDs.
    enum class Register : u16 {
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
        InService                               = 0x100,
        TriggerMode                             = 0x180,
        InterruptRequest                        = 0x200,
        ErrorStatus                             = 0x280,
        InterruptCommand                        = 0x300,
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

    // Write mask for each writable register.
    enum class WriteMask : u32 {
        AllBits                                 = 0xffffffff,
        TaskPriority                            = 0xff,
        EndOfInterrupt                          = AllBits,
        LogicalDestination                      = 0xff000000,
        DestinationFormat                       = 0xf0000000,
        SpuriousInterruptVector                 = 0x3ff,
        ErrorStatus                             = 0xec,
        InterruptCommandHigh                    = 0xff000000,
        InterruptCommandLow                     = 0xccfff,
        TimerLocalVectorTableEntry              = 0x300ff,
        ThermalLocalVectorTableEntry            = 0x107ff,
        PerformanceCounterLocalVectorTableEntry = 0x107ff,
        LocalInterrupt0VectorTableEntry         = 0x187ff,
        LocalInterrupt1VectorTableEntry         = 0x187ff,
        ErrorVectorTableEntry                   = 0x107ff,
        TimerInitialCount                       = AllBits,
        TimerDivideConfiguration                = 0b1011,
    };

    // The base address of the local APIC.
    PhyAddr const m_base;

    // Read a register from the local APIC.
    // @param reg: The register to read from.
    // @return: The current value of the register.
    u32 readRegister(Register const reg) const;

    // Write a register into the local APIC.
    // @param reg: The register to write.
    // @param value: The value to write into the register.
    // @param mask: Indicates which bits of the value should be written to the
    // register.
    void writeRegister(Register const reg,
                       u32 const value,
                       WriteMask const mask = WriteMask::AllBits);
};

// Get a reference to the Local APIC of this cpu.
// @return: Reference to the local APIC of this cpu.
Lapic& lapic();
}
