// Misc functions to directly interact with the CPU. Typically wrapper around
// specific instructions.

#include <cpu/cpu.hpp>
#include <util/panic.hpp>

namespace Cpu {

// Create a table descriptor.
// @param base: The base virtual address of the table.
// @param limit: The size of the table in bytes.
TableDesc::TableDesc(u64 const base, u16 const limit) : limit(limit),
    base(base) {
    // The LGDT instruction expects the limit to be of the form 8*N - 1.
    // Enforce this.
    if (limit % 8 != 7) {
        PANIC("Invalid limit for TableDesc: {}", limit);
    }
}

// Load a GDT using the LGDT instruction. Implemented in assembly.
// @param desc: Pointer to Table descriptor for the GDT to be loaded.
extern "C" void _lgdt(TableDesc const * const desc);

// Load a GDT using the LGDT instruction.
// @param desc: Table descriptor for the GDT to be loaded.
void lgdt(TableDesc const& desc) {
    _lgdt(&desc);
}

// Read the current value loaded in GDTR. Implemented in assembly.
// @param destBase: Where the base of the GDT should be stored.
// @param destLimit: Where the limit of the GDT should be stored.
extern "C" void _sgdt(u64 * const destBase, u16 * const destLimit);

// Read the current value stored in the GDTR using the SGDT instruction.
// @return: The current value loaded in GDTR.
TableDesc sgdt() {
    u64 base;
    u16 limit;
    _sgdt(&base, &limit);
    return TableDesc(base, limit);
}


// Create a segment selector value.
// @param selectorIndex: The index of the segment to point to in the
// GDT/LDT.
// @param useLdt: If true, create a segment selector that reads from the
// LDT, otherwise reads from the GDT.
// @param rpl: The requested privilege level.
SegmentSel::SegmentSel(u16 const selectorIndex,
                       bool const useLdt,
                       PrivLevel const rpl) :
    m_raw((selectorIndex << 3) | (((u64)useLdt) << 2) | static_cast<u8>(rpl)) {
    // Check the params. FIXME: Eventually we should introduce value types that
    // would make this unecessary.
    if (selectorIndex >= 8192) {
        PANIC("Invalid selector index {}", selectorIndex);
    } else if (useLdt) {
        // Not clear if we will ever support/need LDTs in this kernel.
        PANIC("LDT not currently supported");
    }
}

// Create a segment selector value from a raw u16. Useful to parse the
// different fields.
// @param raw: The raw/hardware u16 to build the SegmentSel from.
SegmentSel::SegmentSel(u16 const raw) : m_raw(raw) {}

// Get the selector index of this segment selector.
u16 SegmentSel::selectorIndex() const {
    return m_raw >> 3;
}

// Check if this selector is referring to the LDT.
bool SegmentSel::useLdt() const {
    return !!(m_raw & (1 << 2));
}

// Get the rpl of this selector.
PrivLevel SegmentSel::rpl() const {
    return static_cast<PrivLevel>(m_raw & 3);
}

// Get the raw value of the selector, as expected by the hardware.
u16 SegmentSel::raw() const {
    return m_raw;
}

// Set the segment reg Xs to the value sel. Implemented in assembly.
// @param sel: The new value for Xs.
extern "C" void _setCs(u16 const sel);
extern "C" void _setDs(u16 const sel);
extern "C" void _setEs(u16 const sel);
extern "C" void _setFs(u16 const sel);
extern "C" void _setGs(u16 const sel);
extern "C" void _setSs(u16 const sel);

// Set the value of a segment register.
// @param reg: Select which register to write.
// @param sel: The value to write in the segment register.
void writeSegmentReg(SegmentReg const reg, SegmentSel const sel) {
    switch(reg) {
        case SegmentReg::Cs: _setCs(sel.raw()); break;
        case SegmentReg::Ds: _setDs(sel.raw()); break;
        case SegmentReg::Es: _setEs(sel.raw()); break;
        case SegmentReg::Fs: _setFs(sel.raw()); break;
        case SegmentReg::Gs: _setGs(sel.raw()); break;
        case SegmentReg::Ss: _setSs(sel.raw()); break;
    }
}

// Get the current value of the segment reg Xs. Implemented in assembly.
extern "C" u16 _getCs();
extern "C" u16 _getDs();
extern "C" u16 _getEs();
extern "C" u16 _getFs();
extern "C" u16 _getGs();
extern "C" u16 _getSs();

// Read the value of a segment register.
// @param reg: Select which register to read.
SegmentSel readSegmentReg(SegmentReg const reg) {
    switch(reg) {
        case SegmentReg::Cs: return _getCs(); break;
        case SegmentReg::Ds: return _getDs(); break;
        case SegmentReg::Es: return _getEs(); break;
        case SegmentReg::Fs: return _getFs(); break;
        case SegmentReg::Gs: return _getGs(); break;
        case SegmentReg::Ss: return _getSs(); break;
    }
}
}
