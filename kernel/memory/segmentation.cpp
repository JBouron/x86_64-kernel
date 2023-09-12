// Everything related to segmentation.
// Despite being mostly disabled in 64-bit mode, we still need to setup the GDT.

#include <util/util.hpp>
#include <util/panic.hpp>
#include <memory/segmentation.hpp>

namespace Memory::Segmentation {
// Constructor meant to be called by deriving classes.
// @param base: The base of the descriptor.
// @param limit: The limit of the descriptor.
// @param dpl: The dpl of the descriptor.
// @param type: The type of the descriptor.
// @param mode: Controls if this descriptor is a 16, 32 or 64-bit code/data
// segment.
// @param gran: The granularity of the descriptor.
Descriptor::Descriptor(Base const base,
                       Limit const limit,
                       Cpu::PrivLevel const dpl,
                       Type const type,
                       Mode const mode,
                       Granularity const gran) :
    m_raw((u64(base & 0xff000000) << 32)
          | (static_cast<u64>(gran) << 55)
          | (((mode == Mode::Bits32) ? 1ULL : 0ULL) << 54)
          | (((mode == Mode::Bits64) ? 1ULL : 0ULL) << 53)
          | (((limit.raw() >> 16) & 0xf) << 48)
          | (1ULL << 47)
          | (static_cast<u64>(dpl) << 45)
          | (1ULL << 44)
          | (static_cast<u64>(type) << 40)
          | (u64((base >> 16) & 0xff) << 32)
          | (u64(base & 0xffff) << 16)
          | (u64(limit.raw() & 0xffff))) {}

// Get the raw value of this descriptor.
u64 Descriptor::raw() const {
    return m_raw;
}

// Create a 32-bit segment descriptor.
// @param base: The base of the descriptor.
// @param limit: The limit of the descriptor.
// @param dpl: The dpl of the descriptor.
// @param type: The type of the descriptor.
// @param gran: The granularity of the descriptor.
Descriptor32::Descriptor32(Base const base,
                           Limit const limit,
                           Cpu::PrivLevel const dpl,
                           Type const type,
                           Granularity const gran) :
    Descriptor(base, limit, dpl, type, Mode::Bits32, gran) {}

// Create a 64-bit segment descriptor.
// @param dpl: The dpl of the descriptor.
// @param type: The type of the descriptor.
Descriptor64::Descriptor64(Cpu::PrivLevel const dpl, Type const type) :
    Descriptor(0, Limit(0), dpl, type, Mode::Bits64, Granularity::Byte) {}

// The GDT to be used throughout the entire kernel.
static Descriptor GDT[] = {
    // NULL segment descriptor.
    Descriptor(),
    // Kernel code segment.
    Descriptor64(Cpu::PrivLevel::Ring0, Descriptor::Type::CodeExecuteReadable),
    // Kernel data segment.
    Descriptor64(Cpu::PrivLevel::Ring0, Descriptor::Type::DataReadWrite),
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
