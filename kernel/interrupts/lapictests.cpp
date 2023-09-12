// Test for the local APIC interface.
#include <interrupts/lapic.hpp>
#include <paging/paging.hpp>
#include <framealloc/framealloc.hpp>
#include <util/assert.hpp>

namespace Interrupts {

// Check the value of all the constants and enum in the Lapic interface (both
// public and private). This is mostly used to catch fat-finger mistakes
// changing a const.
SelfTests::TestResult lapicConstantsTest() {
#define CHECK(type, value, exp) \
    TEST_ASSERT(static_cast<type>(Lapic::value) == exp)

    CHECK(u8, DestFmtModel::Cluster,    0x0);
    CHECK(u8, DestFmtModel::Flat,       0xf);

    CHECK(u8, TriggerMode::EdgeTriggered,   0);
    CHECK(u8, TriggerMode::LevelTriggered,  1);

    CHECK(u8, InterruptCmd::MessageType::Fixed,             0b000);
    CHECK(u8, InterruptCmd::MessageType::LowestPriority,    0b001);
    CHECK(u8, InterruptCmd::MessageType::Smi,               0b010);
    CHECK(u8, InterruptCmd::MessageType::RemoteRead,        0b011);
    CHECK(u8, InterruptCmd::MessageType::Nmi,               0b100);
    CHECK(u8, InterruptCmd::MessageType::Init,              0b101);
    CHECK(u8, InterruptCmd::MessageType::Startup,           0b110);
    CHECK(u8, InterruptCmd::MessageType::External,          0b111);

    CHECK(u8, InterruptCmd::DestinationMode::Physical,  0);
    CHECK(u8, InterruptCmd::DestinationMode::Logical,   1);

    CHECK(u8, InterruptCmd::ReadRemoteStatus::InvalidRead,      0b00);
    CHECK(u8, InterruptCmd::ReadRemoteStatus::DeliveryPending,  0b01);
    CHECK(u8, InterruptCmd::ReadRemoteStatus::DataAvailable,    0b10);

    CHECK(u8, InterruptCmd::DestinationShorthand::DestinationField, 0b00);
    CHECK(u8, InterruptCmd::DestinationShorthand::Self,             0b01);
    CHECK(u8, InterruptCmd::DestinationShorthand::AllIncludingSelf, 0b10);
    CHECK(u8, InterruptCmd::DestinationShorthand::AllExcludingSelf, 0b11);

    CHECK(u8, Lvt::MessageType::Fixed,      0b000);
    CHECK(u8, Lvt::MessageType::Smi,        0b010);
    CHECK(u8, Lvt::MessageType::Nmi,        0b100);
    CHECK(u8, Lvt::MessageType::External,   0b111);

    CHECK(u8, Lvt::TimerMode::OneShot,      0);
    CHECK(u8, Lvt::TimerMode::Periodic,     1);

    CHECK(u8, TimerDivideConfiguration::DivideBy2  , 0b0000);
    CHECK(u8, TimerDivideConfiguration::DivideBy4  , 0b0001);
    CHECK(u8, TimerDivideConfiguration::DivideBy8  , 0b0010);
    CHECK(u8, TimerDivideConfiguration::DivideBy16 , 0b0011);
    CHECK(u8, TimerDivideConfiguration::DivideBy32 , 0b1000);
    CHECK(u8, TimerDivideConfiguration::DivideBy64 , 0b1001);
    CHECK(u8, TimerDivideConfiguration::DivideBy128, 0b1010);
    CHECK(u8, TimerDivideConfiguration::DivideBy1  , 0b1011);

    CHECK(u16, Register::ApicId                                 , 0x020);
    CHECK(u16, Register::ApicVersion                            , 0x030);
    CHECK(u16, Register::TaskPriority                           , 0x080);
    CHECK(u16, Register::ArbitrationPriority                    , 0x090);
    CHECK(u16, Register::ProcessorPriority                      , 0x0a0);
    CHECK(u16, Register::EndOfInterrupt                         , 0x0b0);
    CHECK(u16, Register::RemoteRead                             , 0x0c0);
    CHECK(u16, Register::LogicalDestination                     , 0x0d0);
    CHECK(u16, Register::DestinationFormat                      , 0x0e0);
    CHECK(u16, Register::SpuriousInterruptVector                , 0x0f0);
    CHECK(u16, Register::InService                              , 0x100);
    CHECK(u16, Register::TriggerMode                            , 0x180);
    CHECK(u16, Register::InterruptRequest                       , 0x200);
    CHECK(u16, Register::ErrorStatus                            , 0x280);
    CHECK(u16, Register::InterruptCommand                       , 0x300);
    CHECK(u16, Register::TimerLocalVectorTableEntry             , 0x320);
    CHECK(u16, Register::ThermalLocalVectorTableEntry           , 0x330);
    CHECK(u16, Register::PerformanceCounterLocalVectorTableEntry, 0x340);
    CHECK(u16, Register::LocalInterrupt0VectorTableEntry        , 0x350);
    CHECK(u16, Register::LocalInterrupt1VectorTableEntry        , 0x360);
    CHECK(u16, Register::ErrorVectorTableEntry                  , 0x370);
    CHECK(u16, Register::TimerInitialCount                      , 0x380);
    CHECK(u16, Register::TimerCurrentCount                      , 0x390);
    CHECK(u16, Register::TimerDivideConfiguration               , 0x3e0);

    CHECK(u32, WriteMask::AllBits                                , 0xffffffff);
    CHECK(u32, WriteMask::TaskPriority                           , 0xff);
    CHECK(u32, WriteMask::EndOfInterrupt                         , 0xffffffff);
    CHECK(u32, WriteMask::LogicalDestination                     , 0xff000000);
    CHECK(u32, WriteMask::DestinationFormat                      , 0xf0000000);
    CHECK(u32, WriteMask::SpuriousInterruptVector                , 0x3ff);
    CHECK(u32, WriteMask::ErrorStatus                            , 0xec);
    CHECK(u32, WriteMask::InterruptCommandHigh                   , 0xff000000);
    CHECK(u32, WriteMask::InterruptCommandLow                    , 0xccfff);
    CHECK(u32, WriteMask::TimerLocalVectorTableEntry             , 0x300ff);
    CHECK(u32, WriteMask::ThermalLocalVectorTableEntry           , 0x107ff);
    CHECK(u32, WriteMask::PerformanceCounterLocalVectorTableEntry, 0x107ff);
    CHECK(u32, WriteMask::LocalInterrupt0VectorTableEntry        , 0x187ff);
    CHECK(u32, WriteMask::LocalInterrupt1VectorTableEntry        , 0x187ff);
    CHECK(u32, WriteMask::ErrorVectorTableEntry                  , 0x107ff);
    CHECK(u32, WriteMask::TimerInitialCount                      , 0xffffffff);
    CHECK(u32, WriteMask::TimerDivideConfiguration               , 0b1011);
    return SelfTests::TestResult::Success;
#undef CHECK
}

// Check that all the sub-types of Lapic (PriorityInfo, Lvt, ...) have their raw
// values computed correctly.
SelfTests::TestResult lapicRawValuesTest() {
    // Priority Info.
    Lapic::PriorityInfo const pInfo({
        .prioritySubClass = Lapic::PriorityInfo::PrioritySubClass(0x9),
        .priority = Lapic::PriorityInfo::Priority(0x6),
    });
    TEST_ASSERT(pInfo.raw() == 0x69);

    // SpuriousInterrupt.
    bool const apicSoftEna[] = { true, false };
    bool const focusCpu[] = { true, false };
    for (bool const ase : apicSoftEna) {
        for (bool const focus : focusCpu) {
            Lapic::SpuriousInterrupt const spur({
                .vector = Vector(0x69),
                .apicSoftwareEnable = ase,
                .focusCpuCoreScheduling = focus,
            });
            u32 const exp((static_cast<u32>(focus) << 9)
                          | (static_cast<u32>(ase) << 8)
                          | 0x69);
            TEST_ASSERT(spur.raw() == exp);
        }
    }

    // Error Status.
    bool const sentAcceptError[] = { true, false };
    bool const receiveAcceptError[] = { true, false };
    bool const sentIllegalVector[] = { true, false };
    bool const receivedIllegalVector[] = { true, false };
    bool const illegalRegsiterAddress[] = { true, false };
    for (bool const sae : sentAcceptError) {
        for (bool const rae : receiveAcceptError) {
            for (bool const siv : sentIllegalVector) {
                for (bool const riv : receivedIllegalVector) {
                    for (bool const ira : illegalRegsiterAddress) {
                        Lapic::ErrorStatus const status({
                            .sentAcceptError = sae,
                            .receiveAcceptError = rae,
                            .sentIllegalVector = siv,
                            .receivedIllegalVector = riv,
                            .illegalRegsiterAddress = ira,
                        });
                        u32 const exp((static_cast<u32>(ira) << 7)
                                      | (static_cast<u32>(riv) << 6)
                                      | (static_cast<u32>(siv) << 5)
                                      | (static_cast<u32>(rae) << 3)
                                      | (static_cast<u32>(sae) << 2));
                        TEST_ASSERT(status.raw() == exp);
                    }
                }
            }
        }
    }

    // InterruptCmd. Some combinations are technically invalid, however this is
    // ok here as we are merely computing the raw value and not writing them.
    Lapic::InterruptCmd::MessageType const messageType[] = {
        Lapic::InterruptCmd::MessageType::Fixed,
        Lapic::InterruptCmd::MessageType::LowestPriority,
        Lapic::InterruptCmd::MessageType::Smi,
        Lapic::InterruptCmd::MessageType::RemoteRead,
        Lapic::InterruptCmd::MessageType::Nmi,
        Lapic::InterruptCmd::MessageType::Init,
        Lapic::InterruptCmd::MessageType::Startup,
        Lapic::InterruptCmd::MessageType::External,
    };

    Lapic::InterruptCmd::DestinationMode const destMode[] = {
        Lapic::InterruptCmd::DestinationMode::Physical,
        Lapic::InterruptCmd::DestinationMode::Logical,
    };

    bool const deliveryStatus[] = { true, false };
    bool const level[] = { true, false };
    Lapic::TriggerMode const triggerMode[] = {
        Lapic::TriggerMode::EdgeTriggered,
        Lapic::TriggerMode::LevelTriggered,
    };
    Lapic::InterruptCmd::ReadRemoteStatus const readRemoteStatus[] = {
        Lapic::InterruptCmd::ReadRemoteStatus::InvalidRead,
        Lapic::InterruptCmd::ReadRemoteStatus::DeliveryPending,
        Lapic::InterruptCmd::ReadRemoteStatus::DataAvailable,
    };
    Lapic::InterruptCmd::DestinationShorthand const destShorthand[] = {
        Lapic::InterruptCmd::DestinationShorthand::DestinationField,
        Lapic::InterruptCmd::DestinationShorthand::Self,
        Lapic::InterruptCmd::DestinationShorthand::AllIncludingSelf,
        Lapic::InterruptCmd::DestinationShorthand::AllExcludingSelf,
    };
#define SC(type, value) (static_cast<type>((value)))
    for (auto const msgType : messageType) {
        for (auto const dstMode : destMode) {
            for (auto const delStat : deliveryStatus) {
                for (auto const l : level) {
                    for (auto const tMode : triggerMode) {
                        for (auto const rrs : readRemoteStatus) {
                            for (auto const dsh : destShorthand) {
                                Lapic::InterruptCmd  const icr({
                                    .vector = Vector(0xbe),
                                    .messageType = msgType,
                                    .destinationMode = dstMode,
                                    .deliveryStatus = delStat,
                                    .level = l,
                                    .triggerMode = tMode,
                                    .readRemoteStatus = rrs,
                                    .destinationShorthand = dsh,
                                    .destination = 0x69
                                });
                                u64 const exp((SC(u64, 0x69) << 56)
                                              | (SC(u64, dsh) << 18)
                                              | (SC(u64, rrs) << 16)
                                              | (SC(u64, tMode) << 15)
                                              | (SC(u64, l) << 14)
                                              | (SC(u64, delStat) << 12)
                                              | (SC(u64, dstMode) << 11)
                                              | (SC(u64, msgType) << 8)
                                              | 0xbe);
                                TEST_ASSERT(exp == icr.raw());
                            }
                        }
                    }
                }
            }
        }
    }

    // Lvt. Some combinations are technically invalid, however this is
    // ok here as we are merely computing the raw value and not writing them.
    Lapic::Lvt::MessageType const messageTypeLvt[] = {
        Lapic::Lvt::MessageType::Fixed,
        Lapic::Lvt::MessageType::Smi,
        Lapic::Lvt::MessageType::Nmi,
        Lapic::Lvt::MessageType::External,
    };
    bool const remoteIrr[] = { true, false };
    bool const mask[] = { true, false };
    Lapic::Lvt::TimerMode const timerMode[] = {
        Lapic::Lvt::TimerMode::OneShot,
        Lapic::Lvt::TimerMode::Periodic,
    };

    for (auto const msgType : messageTypeLvt) {
        for (auto const delStat : deliveryStatus) {
            for (auto const rir : remoteIrr) {
                for (auto const trgMode : triggerMode) {
                    for (auto const m : mask) {
                        for (auto const tmrMode : timerMode) {
                            Lapic::Lvt const lvt({
                                .vector = Vector(0x69),
                                .messageType = msgType,
                                .deliveryStatus = delStat,
                                .remoteIRR = rir,
                                .triggerMode = trgMode,
                                .mask = m,
                                .timerMode = tmrMode
                            });
                            u32 const exp((SC(u64, tmrMode) << 17)
                                          | (SC(u64, m) << 16)
                                          | (SC(u64, trgMode) << 15)
                                          | (SC(u64, rir) << 14)
                                          | (SC(u64, delStat) << 12)
                                          | (SC(u64, msgType) << 8)
                                          | 0x69);
                            TEST_ASSERT(lvt.raw() == exp);
                        }
                    }
                }
            }
        }
    }
#undef SC
    return SelfTests::TestResult::Success;
}

// RAII-style class that creates a mock apic with an allocated backend page
// frame. The frame is free'ed when this class is destructed.
class MockLapicGuard {
public:
    MockLapicGuard() {
        Res<FrameAlloc::Frame> const alloc(FrameAlloc::alloc());
        ASSERT(alloc.ok());
        m_base = alloc->phyOffset();
        lapic = new Lapic(m_base);
    }

    ~MockLapicGuard() {
        delete lapic;
        // Revert the changes made to the page-table attributes in the Lapic
        // constructor.
        VirAddr const vaddr(m_base.toVir());
        Paging::PageAttr const attr(Paging::PageAttr::Writable);
        Paging::map(vaddr, m_base, attr, 1);
        FrameAlloc::free(FrameAlloc::Frame(m_base.raw()));
    }

    PhyAddr m_base;
    Lapic* lapic;
};

// Helpful shortcut for raw access to a Lapic register. This can be an l-value
// or r-value.
// @param lapic: The Lapic for which to modify the register.
// @param reg: The Lapic::Register to access.
#define REG_PTR(lapic, reg) \
    *((lapic.m_base.toVir() + static_cast<u16>(reg)).ptr<u32>())

// Test reading from the Lapic through the public interface.
SelfTests::TestResult lapicReadTest() {
    MockLapicGuard guard;
    Lapic& lapic(*guard.lapic);

    // Apic ID.
    REG_PTR(lapic, Lapic::Register::ApicId) = (0xbeUL << 24);
    TEST_ASSERT(lapic.apicId() == 0xbe);

    // Version.
    REG_PTR(lapic, Lapic::Register::ApicVersion) = (1ULL << 31)
                                                   | (0xab << 16)
                                                   | 0x74;
    Lapic::VersionInfo const vInfo(lapic.version());
    TEST_ASSERT(vInfo.version == 0x74);
    TEST_ASSERT(vInfo.maxLvtEntries == 0xab);
    TEST_ASSERT(vInfo.hasExtendedApicRegisters == true);

    // Task Priority.
    REG_PTR(lapic, Lapic::Register::TaskPriority) = 0xdf;
    Lapic::PriorityInfo const taskPrio(lapic.taskPriority());
    TEST_ASSERT(taskPrio.priority == 0xd);
    TEST_ASSERT(taskPrio.prioritySubClass == 0xf);

    // Arbitration Priority.
    REG_PTR(lapic, Lapic::Register::ArbitrationPriority) = 0xab;
    Lapic::PriorityInfo const arbPrio(lapic.arbitrationPriority());
    TEST_ASSERT(arbPrio.priority == 0xa);
    TEST_ASSERT(arbPrio.prioritySubClass == 0xb);

    // Processor Priority.
    REG_PTR(lapic, Lapic::Register::ProcessorPriority) = 0xcd;
    Lapic::PriorityInfo const procPrio(lapic.processorPriority());
    TEST_ASSERT(procPrio.priority == 0xc);
    TEST_ASSERT(procPrio.prioritySubClass == 0xd);

    // Remote read.
    REG_PTR(lapic, Lapic::Register::RemoteRead) = 0xabcdef01;
    TEST_ASSERT(lapic.remoteRead() == 0xabcdef01);

    // Logical Destination.
    REG_PTR(lapic, Lapic::Register::LogicalDestination) = (0xaf << 24);
    TEST_ASSERT(lapic.logicalDestination() == 0xaf);

    // Destination Format.
    REG_PTR(lapic, Lapic::Register::DestinationFormat) = (0xf << 28);
    TEST_ASSERT(lapic.destinationFormat() == Lapic::DestFmtModel::Flat);
    REG_PTR(lapic, Lapic::Register::DestinationFormat) = 0x0;
    TEST_ASSERT(lapic.destinationFormat() == Lapic::DestFmtModel::Cluster);

    // Spurious Interrupt Vector.
    REG_PTR(lapic, Lapic::Register::SpuriousInterruptVector) = (1ULL << 9)|0xbd;
    Lapic::SpuriousInterrupt const spur1(lapic.spuriousInterrupt());
    TEST_ASSERT(spur1.vector == 0xbd);
    TEST_ASSERT(!spur1.apicSoftwareEnable);
    TEST_ASSERT(spur1.focusCpuCoreScheduling);
    REG_PTR(lapic, Lapic::Register::SpuriousInterruptVector) = (1ULL << 8)|0xed;
    Lapic::SpuriousInterrupt const spur2(lapic.spuriousInterrupt());
    TEST_ASSERT(spur2.vector == 0xed);
    TEST_ASSERT(spur2.apicSoftwareEnable);
    TEST_ASSERT(!spur2.focusCpuCoreScheduling);

    // ISR.
    u32 * const isrBase((lapic.m_base.toVir()
                         + static_cast<u16>(Lapic::Register::InService))
                        .ptr<u32>());
    for (u64 i(0); i < 8; ++i) {
        // Index is i*4 because registers are 16-bytes aligned.
        isrBase[i * 4] = 0xdeadbeef * (i + 1);
    }
    Lapic::Bitmap const isr(lapic.inService());
    for (u64 i(0); i < 8; ++i) {
        u32 const exp(0xdeadbeef * (i + 1));
        TEST_ASSERT(isr.dword[i] == exp);
    }

    // TMR.
    u32 * const tmrBase((lapic.m_base.toVir()
                         + static_cast<u16>(Lapic::Register::TriggerMode))
                        .ptr<u32>());
    for (u64 i(0); i < 8; ++i) {
        // Index is i*4 because registers are 16-bytes aligned.
        tmrBase[i * 4] = 0xcafebabe * (i + 1);
    }
    Lapic::Bitmap const tmr(lapic.triggerMode());
    for (u64 i(0); i < 8; ++i) {
        u32 const exp(0xcafebabe * (i + 1));
        TEST_ASSERT(tmr.dword[i] == exp);
    }

    // TMR.
    u32 * const irrBase((lapic.m_base.toVir()
                         + static_cast<u16>(Lapic::Register::InterruptRequest))
                        .ptr<u32>());
    for (u64 i(0); i < 8; ++i) {
        // Index is i*4 because registers are 16-bytes aligned.
        irrBase[i * 4] = 0xbaadcafe * (i + 1);
    }
    Lapic::Bitmap const irr(lapic.interruptRequest());
    for (u64 i(0); i < 8; ++i) {
        u32 const exp(0xbaadcafe * (i + 1));
        TEST_ASSERT(irr.dword[i] == exp);
    }

    // Error Status.
    bool const sentAcceptError[] = { true, false };
    bool const receiveAcceptError[] = { true, false };
    bool const sentIllegalVector[] = { true, false };
    bool const receivedIllegalVector[] = { true, false };
    bool const illegalRegsiterAddress[] = { true, false };
    for (bool const sae : sentAcceptError) {
        for (bool const rae : receiveAcceptError) {
            for (bool const siv : sentIllegalVector) {
                for (bool const riv : receivedIllegalVector) {
                    for (bool const ira : illegalRegsiterAddress) {
                        Lapic::ErrorStatus const status({
                            .sentAcceptError = sae,
                            .receiveAcceptError = rae,
                            .sentIllegalVector = siv,
                            .receivedIllegalVector = riv,
                            .illegalRegsiterAddress = ira,
                        });
                        REG_PTR(lapic, Lapic::Register::ErrorStatus) =
                            status.raw();
                        Lapic::ErrorStatus const read(lapic.errorStatus());
                        TEST_ASSERT(read == status);
                    }
                }
            }
        }
    }

    // InterruptCmd. Some combinations are technically invalid, however this is
    // ok here as we are not interacting with a real LAPIC.
    Lapic::InterruptCmd::MessageType const messageType[] = {
        Lapic::InterruptCmd::MessageType::Fixed,
        Lapic::InterruptCmd::MessageType::LowestPriority,
        Lapic::InterruptCmd::MessageType::Smi,
        Lapic::InterruptCmd::MessageType::RemoteRead,
        Lapic::InterruptCmd::MessageType::Nmi,
        Lapic::InterruptCmd::MessageType::Init,
        Lapic::InterruptCmd::MessageType::Startup,
        Lapic::InterruptCmd::MessageType::External,
    };

    Lapic::InterruptCmd::DestinationMode const destMode[] = {
        Lapic::InterruptCmd::DestinationMode::Physical,
        Lapic::InterruptCmd::DestinationMode::Logical,
    };

    bool const deliveryStatus[] = { true, false };
    bool const level[] = { true, false };
    Lapic::TriggerMode const triggerMode[] = {
        Lapic::TriggerMode::EdgeTriggered,
        Lapic::TriggerMode::LevelTriggered,
    };
    Lapic::InterruptCmd::ReadRemoteStatus const readRemoteStatus[] = {
        Lapic::InterruptCmd::ReadRemoteStatus::InvalidRead,
        Lapic::InterruptCmd::ReadRemoteStatus::DeliveryPending,
        Lapic::InterruptCmd::ReadRemoteStatus::DataAvailable,
    };
    Lapic::InterruptCmd::DestinationShorthand const destShorthand[] = {
        Lapic::InterruptCmd::DestinationShorthand::DestinationField,
        Lapic::InterruptCmd::DestinationShorthand::Self,
        Lapic::InterruptCmd::DestinationShorthand::AllIncludingSelf,
        Lapic::InterruptCmd::DestinationShorthand::AllExcludingSelf,
    };
    u32 * const icrBase((lapic.m_base.toVir()
                         + static_cast<u16>(Lapic::Register::InterruptCommand))
                        .ptr<u32>());
    for (auto const msgType : messageType) {
        for (auto const dstMode : destMode) {
            for (auto const delStat : deliveryStatus) {
                for (auto const l : level) {
                    for (auto const tMode : triggerMode) {
                        for (auto const rrs : readRemoteStatus) {
                            for (auto const dsh : destShorthand) {
                                Lapic::InterruptCmd  const icr({
                                    .vector = Vector(0xbe),
                                    .messageType = msgType,
                                    .destinationMode = dstMode,
                                    .deliveryStatus = delStat,
                                    .level = l,
                                    .triggerMode = tMode,
                                    .readRemoteStatus = rrs,
                                    .destinationShorthand = dsh,
                                    .destination = 0x69
                                });
                                u64 const raw(icr.raw());
                                u32 const high(raw >> 32);
                                u32 const low(raw & 0xffffffff);
                                icrBase[0] = low;
                                // 4 because registers are 16-bytes aligned.
                                icrBase[4] = high;
                                Lapic::InterruptCmd const read(
                                    lapic.interruptCommand());
                                TEST_ASSERT(read == icr);
                            }
                        }
                    }
                }
            }
        }
    }

    // Timer LVT.
    Lapic::Lvt::MessageType const messageTypeLvt[] = {
        Lapic::Lvt::MessageType::Fixed,
        Lapic::Lvt::MessageType::Smi,
        Lapic::Lvt::MessageType::Nmi,
        Lapic::Lvt::MessageType::External,
    };
    bool const remoteIrr[] = { true, false };
    bool const mask[] = { true, false };
    Lapic::Lvt::TimerMode const timerMode[] = {
        Lapic::Lvt::TimerMode::OneShot,
        Lapic::Lvt::TimerMode::Periodic,
    };

    for (auto const delStat : deliveryStatus) {
        for (auto const m : mask) {
            for (auto const tmrMode : timerMode) {
                Lapic::Lvt const lvt({
                    .vector = Vector(0x69),
                    .deliveryStatus = delStat,
                    .mask = m,
                    .timerMode = tmrMode
                });
                REG_PTR(lapic, Lapic::Register::TimerLocalVectorTableEntry) =
                     lvt.raw();
                Lapic::Lvt const read(lapic.timerLvt());
                TEST_ASSERT(read == lvt);
            }
        }
    }

    // Thermal, Performance, Apic Error LVTs
    for (auto const msgType : messageTypeLvt) {
        for (auto const delStat : deliveryStatus) {
            for (auto const m : mask) {
                Lapic::Lvt lvt({
                    .vector = Vector(0xab),
                    .messageType = msgType,
                    .deliveryStatus = delStat,
                    .mask = m,
                });
                REG_PTR(lapic, Lapic::Register::ThermalLocalVectorTableEntry)
                    = lvt.raw();
                TEST_ASSERT(lapic.thermalLvt() == lvt);

                lvt.vector = Vector(0xac);
                REG_PTR(lapic,
                    Lapic::Register::PerformanceCounterLocalVectorTableEntry) =
                    lvt.raw();
                TEST_ASSERT(lapic.performanceCounterLvt() == lvt);

                lvt.vector = Vector(0xad);
                REG_PTR(lapic, Lapic::Register::ErrorVectorTableEntry) =
                    lvt.raw();
                TEST_ASSERT(lapic.errorLvt() == lvt);
            }
        }
    }

    // LINT0 & LINT1 LVTs.
    for (auto const msgType : messageTypeLvt) {
        for (auto const delStat : deliveryStatus) {
            for (auto const rir : remoteIrr) {
                for (auto const trgMode : triggerMode) {
                    for (auto const m : mask) {
                        Lapic::Lvt lvt({
                            .vector = Vector(0x61),
                            .messageType = msgType,
                            .deliveryStatus = delStat,
                            .remoteIRR = rir,
                            .triggerMode = trgMode,
                            .mask = m,
                        });
                        REG_PTR(lapic,
                            Lapic::Register::LocalInterrupt0VectorTableEntry) =
                            lvt.raw();
                        TEST_ASSERT(lapic.localInterrupt0Lvt() == lvt);

                        lvt.vector = Vector(0x60);
                        REG_PTR(lapic,
                            Lapic::Register::LocalInterrupt1VectorTableEntry) =
                            lvt.raw();
                        TEST_ASSERT(lapic.localInterrupt1Lvt() == lvt);
                    }
                }
            }
        }
    }

    // Timer Initial Count.
    REG_PTR(lapic, Lapic::Register::TimerInitialCount) = 0xbeefbabe;
    TEST_ASSERT(lapic.timerInitialCount() == 0xbeefbabe);

    // Timer Current Count.
    REG_PTR(lapic, Lapic::Register::TimerCurrentCount) = 0xfac70210;
    TEST_ASSERT(lapic.timerCurrentCount() == 0xfac70210);

    // Timer Divide Configuration.
    Lapic::TimerDivideConfiguration const divConfig[] = {
        Lapic::TimerDivideConfiguration::DivideBy2,
        Lapic::TimerDivideConfiguration::DivideBy4,
        Lapic::TimerDivideConfiguration::DivideBy8,
        Lapic::TimerDivideConfiguration::DivideBy16,
        Lapic::TimerDivideConfiguration::DivideBy32,
        Lapic::TimerDivideConfiguration::DivideBy64,
        Lapic::TimerDivideConfiguration::DivideBy128,
        Lapic::TimerDivideConfiguration::DivideBy1,
    };
    for (Lapic::TimerDivideConfiguration const div : divConfig) {
        REG_PTR(lapic, Lapic::Register::TimerDivideConfiguration) =
            static_cast<u32>(div);
        TEST_ASSERT(lapic.timerDivideConfiguration() == div);
    }

    return SelfTests::TestResult::Success;
}

// Test writing to the Lapic through the public interface.
SelfTests::TestResult lapicWriteTest() {
    MockLapicGuard guard;
    Lapic& lapic(*guard.lapic);

    // Task priority.
    Lapic::PriorityInfo const taskPrio({
        .prioritySubClass = 0xa,
        .priority = 0xb,
    });
    lapic.setTaskPriority(taskPrio);
    TEST_ASSERT(lapic.taskPriority() == taskPrio);

    // Logical destination.
    lapic.setLogicalDestination(0x69);
    TEST_ASSERT(lapic.logicalDestination() == 0x69);

    // Destination format.
    lapic.setDestinationFormat(Lapic::DestFmtModel::Flat);
    TEST_ASSERT(lapic.destinationFormat() == Lapic::DestFmtModel::Flat);
    lapic.setDestinationFormat(Lapic::DestFmtModel::Cluster);
    TEST_ASSERT(lapic.destinationFormat() == Lapic::DestFmtModel::Cluster);

    // Spurious Interrupt.
    Lapic::SpuriousInterrupt spur({
        .vector = Vector(0xde),
        .apicSoftwareEnable = true,
        .focusCpuCoreScheduling = false,
    });
    lapic.setSpuriousInterrupt(spur);
    TEST_ASSERT(lapic.spuriousInterrupt() == spur);
    spur.apicSoftwareEnable = false;
    spur.focusCpuCoreScheduling = true;
    lapic.setSpuriousInterrupt(spur);
    TEST_ASSERT(lapic.spuriousInterrupt() == spur);

    // Error Status.
    bool const sentAcceptError[] = { true, false };
    bool const receiveAcceptError[] = { true, false };
    bool const sentIllegalVector[] = { true, false };
    bool const receivedIllegalVector[] = { true, false };
    bool const illegalRegsiterAddress[] = { true, false };
    for (bool const sae : sentAcceptError) {
        for (bool const rae : receiveAcceptError) {
            for (bool const siv : sentIllegalVector) {
                for (bool const riv : receivedIllegalVector) {
                    for (bool const ira : illegalRegsiterAddress) {
                        Lapic::ErrorStatus const status({
                            .sentAcceptError = sae,
                            .receiveAcceptError = rae,
                            .sentIllegalVector = siv,
                            .receivedIllegalVector = riv,
                            .illegalRegsiterAddress = ira,
                        });
                        lapic.setErrorStatus(status);
                        TEST_ASSERT(lapic.errorStatus() == status);
                    }
                }
            }
        }
    }

    // InterruptCmd. Some combinations are technically invalid, however this is
    // ok here as we are not interacting with a real LAPIC.
    Lapic::InterruptCmd::MessageType const messageType[] = {
        Lapic::InterruptCmd::MessageType::Fixed,
        Lapic::InterruptCmd::MessageType::LowestPriority,
        Lapic::InterruptCmd::MessageType::Smi,
        Lapic::InterruptCmd::MessageType::RemoteRead,
        Lapic::InterruptCmd::MessageType::Nmi,
        Lapic::InterruptCmd::MessageType::Init,
        Lapic::InterruptCmd::MessageType::Startup,
        Lapic::InterruptCmd::MessageType::External,
    };

    Lapic::InterruptCmd::DestinationMode const destMode[] = {
        Lapic::InterruptCmd::DestinationMode::Physical,
        Lapic::InterruptCmd::DestinationMode::Logical,
    };

    bool const deliveryStatus[] = { true, false };
    bool const level[] = { true, false };
    Lapic::TriggerMode const triggerMode[] = {
        Lapic::TriggerMode::EdgeTriggered,
        Lapic::TriggerMode::LevelTriggered,
    };
    Lapic::InterruptCmd::ReadRemoteStatus const readRemoteStatus[] = {
        Lapic::InterruptCmd::ReadRemoteStatus::InvalidRead,
        Lapic::InterruptCmd::ReadRemoteStatus::DeliveryPending,
        Lapic::InterruptCmd::ReadRemoteStatus::DataAvailable,
    };
    Lapic::InterruptCmd::DestinationShorthand const destShorthand[] = {
        Lapic::InterruptCmd::DestinationShorthand::DestinationField,
        Lapic::InterruptCmd::DestinationShorthand::Self,
        Lapic::InterruptCmd::DestinationShorthand::AllIncludingSelf,
        Lapic::InterruptCmd::DestinationShorthand::AllExcludingSelf,
    };
    for (auto const msgType : messageType) {
        for (auto const dstMode : destMode) {
            for (auto const delStat : deliveryStatus) {
                for (auto const l : level) {
                    for (auto const tMode : triggerMode) {
                        for (auto const rrs : readRemoteStatus) {
                            for (auto const dsh : destShorthand) {
                                Lapic::InterruptCmd  const icr({
                                    .vector = Vector(0xbe),
                                    .messageType = msgType,
                                    .destinationMode = dstMode,
                                    .deliveryStatus = delStat,
                                    .level = l,
                                    .triggerMode = tMode,
                                    .readRemoteStatus = rrs,
                                    .destinationShorthand = dsh,
                                    .destination = 0x69
                                });
                                lapic.setInterruptCommand(icr);
                                // Some of the bits of the ICR are read-only,
                                // hence not changed by the write op.
                                Lapic::InterruptCmd exp(icr);
                                exp.deliveryStatus = false;
                                exp.readRemoteStatus =
                            Lapic::InterruptCmd::ReadRemoteStatus::InvalidRead;
                                TEST_ASSERT(lapic.interruptCommand() == exp);
                            }
                        }
                    }
                }
            }
        }
    }

    // Timer LVT.
    Lapic::Lvt::MessageType const messageTypeLvt[] = {
        Lapic::Lvt::MessageType::Fixed,
        Lapic::Lvt::MessageType::Smi,
        Lapic::Lvt::MessageType::Nmi,
        Lapic::Lvt::MessageType::External,
    };
    bool const remoteIrr[] = { true, false };
    bool const mask[] = { true, false };
    Lapic::Lvt::TimerMode const timerMode[] = {
        Lapic::Lvt::TimerMode::OneShot,
        Lapic::Lvt::TimerMode::Periodic,
    };

    for (auto const delStat : deliveryStatus) {
        for (auto const m : mask) {
            for (auto const tmrMode : timerMode) {
                Lapic::Lvt const lvt({
                    .vector = Vector(0x69),
                    .deliveryStatus = delStat,
                    .mask = m,
                    .timerMode = tmrMode
                });
                lapic.setTimerLvt(lvt);
                // Make sure not to compare the read-only bits.
                Lapic::Lvt exp(lvt);
                exp.deliveryStatus = false;
                exp.remoteIRR = false;
                TEST_ASSERT(lapic.timerLvt() == exp);
            }
        }
    }

    // Thermal, Performance, Apic Error LVTs
    for (auto const msgType : messageTypeLvt) {
        for (auto const delStat : deliveryStatus) {
            for (auto const m : mask) {
                Lapic::Lvt lvt({
                    .vector = Vector(0xab),
                    .messageType = msgType,
                    .deliveryStatus = delStat,
                    .mask = m,
                });
                // Make sure not to compare the read-only bits.
                Lapic::Lvt exp(lvt);
                exp.deliveryStatus = false;
                exp.remoteIRR = false;

                lapic.setThermalLvt(lvt);
                TEST_ASSERT(lapic.thermalLvt() == exp);

                lvt.vector = Vector(0xac);
                exp.vector = lvt.vector;
                lapic.setPerformanceCounterLvt(lvt);
                TEST_ASSERT(lapic.performanceCounterLvt() == exp);

                lvt.vector = Vector(0xad);
                exp.vector = lvt.vector;
                lapic.setErrorLvt(lvt);
                TEST_ASSERT(lapic.errorLvt() == exp);
            }
        }
    }

    // LINT0 & LINT1 LVTs.
    for (auto const msgType : messageTypeLvt) {
        for (auto const delStat : deliveryStatus) {
            for (auto const rir : remoteIrr) {
                for (auto const trgMode : triggerMode) {
                    for (auto const m : mask) {
                        Lapic::Lvt lvt({
                            .vector = Vector(0x61),
                            .messageType = msgType,
                            .deliveryStatus = delStat,
                            .remoteIRR = rir,
                            .triggerMode = trgMode,
                            .mask = m,
                        });
                        // Make sure not to compare the read-only bits.
                        Lapic::Lvt exp(lvt);
                        exp.deliveryStatus = false;
                        exp.remoteIRR = false;

                        lapic.setLocalInterrupt0Lvt(lvt);
                        TEST_ASSERT(lapic.localInterrupt0Lvt() == exp);

                        lvt.vector = Vector(0x60);
                        exp.vector = lvt.vector;
                        lapic.setLocalInterrupt1Lvt(lvt);
                        TEST_ASSERT(lapic.localInterrupt1Lvt() == exp);
                    }
                }
            }
        }
    }

    // Timer Initial Count.
    lapic.setTimerInitialCount(0xbeefbabe);
    TEST_ASSERT(lapic.timerInitialCount() == 0xbeefbabe);

    // Timer Divide Configuration.
    Lapic::TimerDivideConfiguration const divConfig[] = {
        Lapic::TimerDivideConfiguration::DivideBy2,
        Lapic::TimerDivideConfiguration::DivideBy4,
        Lapic::TimerDivideConfiguration::DivideBy8,
        Lapic::TimerDivideConfiguration::DivideBy16,
        Lapic::TimerDivideConfiguration::DivideBy32,
        Lapic::TimerDivideConfiguration::DivideBy64,
        Lapic::TimerDivideConfiguration::DivideBy128,
        Lapic::TimerDivideConfiguration::DivideBy1,
    };
    for (Lapic::TimerDivideConfiguration const div : divConfig) {
        lapic.setTimerDivideConfiguration(div);
        TEST_ASSERT(lapic.timerDivideConfiguration() == div);
    }

    return SelfTests::TestResult::Success;
}

// Run the Lapic tests.
void Lapic::Test(SelfTests::TestRunner& runner) {
    RUN_TEST(runner, lapicConstantsTest);
    RUN_TEST(runner, lapicRawValuesTest);
    RUN_TEST(runner, lapicReadTest);
    RUN_TEST(runner, lapicWriteTest);
}
}
