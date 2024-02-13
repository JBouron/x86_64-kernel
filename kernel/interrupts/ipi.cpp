// Inter-Processor-Interrupts related routines.
#include <interrupts/ipi.hpp>
#include <interrupts/lapic.hpp>

namespace Interrupts::Ipi {

// Send an Inter-Processor-Interrupt to a cpu.
// @param destinationCpu: The ID of the cpu to be interrupted. This can be the
// ID of the current cpu, in which case the current CPU interrupts itself.
// @param vector: The interrupt vector to raise on the destination cpu.
void sendIpi(Smp::Id const& destinationCpu, Vector const& vector) {
    Lapic::InterruptCmd const cmd({
        .vector = vector,
        .messageType = Lapic::InterruptCmd::MessageType::Fixed,
        .destinationMode = Lapic::InterruptCmd::DestinationMode::Physical,
        .level = false,
        .triggerMode = Lapic::TriggerMode::EdgeTriggered,
        .destinationShorthand =
            Lapic::InterruptCmd::DestinationShorthand::DestinationField,
        // FIXME: The static_cast is needed here because automatic down-cast
        // u64->u32 generates a compilation error. There may be a way to avoid
        // this without a cast.
        .destination = static_cast<u32>(destinationCpu.raw()),
    });
    ASSERT(cmd.isValid());
    Interrupts::lapic().setInterruptCommand(cmd);
}
}
