// Misc functions to directly interact with the CPU. Typically wrapper around
// specific instructions.

#pragma once
#include <util/util.hpp>

namespace Cpu {

// #############################################################################
// Types and functions related to system tables, e.g. GDT, LDT, IDT, ...
// #############################################################################

// Table descriptor for the LGDT instruction.
class TableDesc {
public:
    // Create a table descriptor.
    // @param base: The base virtual address of the table.
    // @param limit: The size of the table in bytes.
    TableDesc(u64 const base, u16 const limit);

    // Compare two TableDescs.
    // @param other: The other TableDesc to compare against.
    // @return: true if this and the other TableDesc have the same base and
    // limit, false otherwise.
    bool operator==(TableDesc const& other) const = default;

private:
    u16 const limit;
    u64 const base;
} __attribute__((packed));
static_assert(sizeof(TableDesc) == 10);

// Load a GDT using the LGDT instruction.
// @param desc: Table descriptor for the GDT to be loaded.
void lgdt(TableDesc const& desc);

// Read the current value stored in the GDTR using the SGDT instruction.
// @return: The current value loaded in GDTR.
TableDesc sgdt();


// #############################################################################
// Types and functions related to segment registers.
// #############################################################################

// CPU privilege level.
enum class PrivLevel : u8 {
    Ring0 = 0,
    Ring1 = 1,
    Ring2 = 2,
    Ring3 = 3,
};

// Segment selector value.
class SegmentSel {
public:
    // Create a segment selector value.
    // @param selectorIndex: The index of the segment to point to in the
    // GDT/LDT.
    // @param useLdt: If true, create a segment selector that reads from the
    // LDT, otherwise reads from the GDT.
    // @param rpl: The requested privilege level.
    SegmentSel(u16 const selectorIndex, bool const useLdt, PrivLevel const rpl);

    // Create a segment selector value referring to the GDT.
    // @param selectorIndex: The index of the segment to point to in the GDT.
    // @param rpl: The requested privilege level.
    SegmentSel(u16 const selectorIndex, PrivLevel const rpl);

    // Create a segment selector value from a raw u16. Useful to parse the
    // different fields.
    // @param raw: The raw/hardware u16 to build the SegmentSel from.
    SegmentSel(u16 const raw);

    // Auto-generated comparison operator.
    bool operator==(SegmentSel const& other) const = default;

    // Get the selector index of this segment selector.
    u16 selectorIndex() const;

    // Check if this selector is referring to the LDT.
    bool useLdt() const;

    // Get the rpl of this selector.
    PrivLevel rpl() const;

    // Get the raw value of the selector, as expected by the hardware.
    u16 raw() const;

private:
    u16 const m_raw;
};
static_assert(sizeof(SegmentSel) == 2);

// Identify a cpu segment register.
enum class SegmentReg {
    Cs,
    Ds,
    Es,
    Fs,
    Gs,
    Ss,
};

// Set the value of a segment register.
// @param reg: Select which register to write.
// @param sel: The value to write in the segment register.
void writeSegmentReg(SegmentReg const reg, SegmentSel const sel);

// Read the value of a segment register.
// @param reg: Select which register to read.
SegmentSel readSegmentReg(SegmentReg const reg);

}
