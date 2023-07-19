// Everything related to segmentation.
// Despite being mostly disabled in 64-bit mode, we still need to setup the GDT.

#include <util/util.hpp>
#include <util/panic.hpp>
#include <memory/segmentation.hpp>

namespace Memory::Segmentation {

// 64-bit Segment Descriptor for the GDT.
class Descriptor {
public:
    // Type of segment descriptor.
    enum class Type {
        Code,
        Data,
    };

    // Descriptor Privilege Level (DPL).
    using Dpl = u8;

    // Create a NULL segment descriptor.
    constexpr Descriptor() : m_raw(0) {}

    // Create a Descriptor instance.
    // @param type: The type of the segment.
    // @param dpl: The descriptor's privilege level. Ignored for data segments.
    constexpr Descriptor(__attribute__((unused)) Type const type,
                         Dpl const dpl) :
    m_raw((1ULL << 53) | (1ULL << 47) | (((u64)dpl) << 35)) {}
    // Note: For now type is not used. However we might need it in the future
    // when declaring 32-bit segments when initialization other cpus.

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
    Descriptor(Descriptor::Type::Code,0),
    // Kernel data segment.
    Descriptor(Descriptor::Type::Data,0),
};

// Table descriptor for the LGDT instruction.
class TableDesc {
public:
    // Create a table descriptor.
    // @param base: The base virtual address of the table.
    // @param limit: The size of the table in bytes.
    TableDesc(u64 const base, u16 const limit) : limit(limit), base(base) {
        // The LGDT instruction expects the limit to be of the form 8*N - 1.
        // Enforce this.
        if (limit % 8 != 7) {
            PANIC("Invalid limit for TableDesc: {}", limit);
        }
    }

private:
    u16 const limit;
    u64 const base;
} __attribute__((packed));
static_assert(sizeof(TableDesc) == 10);

// Load a GDT using the LGDT instruction.
// @param desc: Table descriptor for the GDT to be loaded.
extern "C" void lgdt(TableDesc const * const desc);


// Initialize segmentation. Create a GDT and load it in GDTR.
void Init() {
    Log::debug("Segmentation: Init");
    u64 const gdtBase(reinterpret_cast<u64>(GDT));
    u16 const gdtLimit(sizeof(GDT) - 1);

    Log::info("Loading GDT, base = {x}, limit = {}", gdtBase, gdtLimit);
    TableDesc const desc(gdtBase, gdtLimit);
    lgdt(&desc);
    Log::debug("GDT loaded");
}

}
