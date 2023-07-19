// Misc functions to directly interact with the CPU. Typically wrapper around
// specific instructions.

#pragma once
#include <util/util.hpp>

namespace Cpu {

// Table descriptor for the LGDT instruction.
class TableDesc {
public:
    // Create a table descriptor.
    // @param base: The base virtual address of the table.
    // @param limit: The size of the table in bytes.
    TableDesc(u64 const base, u16 const limit);

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
}
