// Everything related to segmentation.
// Despite being mostly disabled in 64-bit mode, we still need to setup the GDT.

#include <util/util.hpp>
#include <util/panic.hpp>
#include <memory/segmentation.hpp>
#include <cpu/cpu.hpp>

namespace Memory::Segmentation {

// 64-bit Segment Descriptor for the GDT.
class Descriptor {
public:
    // Type of segment descriptor.
    enum class Type {
        Code,
        Data,
    };

    // Create a NULL segment descriptor.
    constexpr Descriptor() : m_raw(0) {}

    // Create a Descriptor instance.
    // @param type: The type of the segment.
    constexpr Descriptor(Type const type) :
        m_raw((1ULL << 53) |
              (1ULL << 47) |
              (1ULL << 44) |
              (type == Type::Code ? (1ULL << 43) : (1ULL << 41))) {}
    // Note: Despite all the "64-bit IgNoReS MoSt oF thE DeScriPtOr BitS" it
    // seems that some bits are still expected to be there. First the types bit
    // and the write bit for SS.

private:
    // The raw 64-bit value of the segment descriptor, as expected by the
    // hardware.
    u64 const m_raw;
} __attribute__((packed));
static_assert(sizeof(Descriptor) == 8);

// The GDT to be used throughout the entire kernel.
static Descriptor GDT[] = {
    // NULL segment descriptor.
    Descriptor(),
    // Kernel code segment.
    Descriptor(Descriptor::Type::Code),
    // Kernel data segment.
    Descriptor(Descriptor::Type::Data),
};

// Initialize segmentation. Create a GDT and load it in GDTR.
void Init() {
    Log::debug("Segmentation: Init");
    u64 const gdtBase(reinterpret_cast<u64>(GDT));
    u16 const gdtLimit(sizeof(GDT) - 1);

    Log::info("Loading GDT, base = {x}, limit = {}", gdtBase, gdtLimit);
    Cpu::TableDesc const desc(gdtBase, gdtLimit);
    Cpu::lgdt(desc);
    Log::debug("GDT loaded");
    
    Cpu::SegmentSel const codeSel(1, Cpu::PrivLevel::Ring0);
    Cpu::SegmentSel const dataSel(2, Cpu::PrivLevel::Ring0);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Cs, codeSel);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Ds, dataSel);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Es, dataSel);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Fs, dataSel);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Gs, dataSel);
    Cpu::writeSegmentReg(Cpu::SegmentReg::Ss, dataSel);
    Log::debug("Setting segment registers");

}

}
