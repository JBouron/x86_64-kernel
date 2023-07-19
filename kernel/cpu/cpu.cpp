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

}
