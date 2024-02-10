// Everything related to segmentation.
// Despite being mostly disabled in 64-bit mode, we still need to setup the GDT.
#pragma once
#include <cpu/cpu.hpp>
#include <util/subrange.hpp>
#include <selftests/selftests.hpp>

namespace Memory::Segmentation {

// Segment Descriptor for the GDT. This is a base class further derived from to
// make creating descriptor easier.
class Descriptor {
public:
    // Type for the base field of the descriptor.
    using Base = u32;

    // Type for the limit field of the descriptor.
    class Limit : public SubRange<Limit, 0, (1ULL << 20) - 1> {};

    // Type of segment descriptor.
    enum class Type : u64 {
        DataReadOnly = 0x0,
        DataReadWrite = 0x2,
        // FIXME: Add support for expand-down segments. This might require some
        // more work to correctly set the D/B bit.
        //DataExpandDownReadOnly = 0x4,
        //DataExpandDownReadWrite = 0x6,
        CodeExecuteOnly = 0x8,
        CodeExecuteReadable = 0xa,
        CodeConformingExecuteOnly = 0xc,
        CodeConformingExecuteReadable = 0xe,
    };

    // Controls the granularity of the descriptor.
    enum class Granularity : u64 {
        Byte = 0,
        Page = 1,
    };

    // Create a NULL segment descriptor.
    Descriptor() : m_raw(0) {}

    // Get the raw value of this descriptor.
    u64 raw() const;

protected:
    // Indirectly controls the value of the D/B and L bits of the descriptor.
    enum class Mode {
        // 64-bit segment: sets the L bit and un-set the D/B bit.
        Bits64,
        // 32-bit segment: unsets the L bit and sets the D/B bit.
        Bits32,
        // 16-bit segment: unsets the L bit and unsets the D/B bit.
        Bits16,
    };

    // Constructor meant to be called by deriving classes.
    // @param base: The base of the descriptor.
    // @param limit: The limit of the descriptor.
    // @param dpl: The dpl of the descriptor.
    // @param type: The type of the descriptor.
    // @param mode: Controls if this descriptor is a 16, 32 or 64-bit code/data
    // segment.
    // @param gran: The granularity of the descriptor.
    Descriptor(Base const base,
               Limit const limit,
               Cpu::PrivLevel const dpl,
               Type const type,
               Mode const mode,
               Granularity const gran);

private:
    // The raw 64-bit value of the segment descriptor, as expected by the
    // hardware.
    u64 const m_raw;
} __attribute__((packed));

// A 32-bit code or data segment descriptor.
class Descriptor32 : public Descriptor {
public:
    // Create a 32-bit segment descriptor.
    // @param base: The base of the descriptor.
    // @param limit: The limit of the descriptor.
    // @param dpl: The dpl of the descriptor.
    // @param type: The type of the descriptor.
    // @param gran: The granularity of the descriptor.
    Descriptor32(Base const base,
                 Limit const limit,
                 Cpu::PrivLevel const dpl,
                 Type const type,
                 Granularity const gran);
} __attribute__((packed));

// A flat 32-bit code or data segment descriptor, e.g. a segment that spans the
// entire 4GiB address space. This is meant to be used as a shortcut for
// Descriptor32.
class Descriptor32Flat : public Descriptor32 {
public:
    // Create a flat 32-bit segment descriptor.
    // @param dpl: The dpl of the descriptor.
    // @param type: The type of the descriptor.
    Descriptor32Flat(Cpu::PrivLevel const dpl, Type const type);
} __attribute__((packed));

// A 64-bit code or data segment descriptor. In 64-bits, most of the fields in
// the segment descriptor are ingnored, hence the lighter constructor.
class Descriptor64 : public Descriptor {
public:
    // Create a 64-bit segment descriptor.
    // @param dpl: The dpl of the descriptor.
    // @param type: The type of the descriptor.
    Descriptor64(Cpu::PrivLevel const dpl, Type const type);
} __attribute__((packed));

// Descriptor values are meant to be stored directly into the GDT so they must
// occupy the same space as the underlying u64.
static_assert(sizeof(Descriptor) == 8);
static_assert(sizeof(Descriptor32) == 8);
static_assert(sizeof(Descriptor64) == 8);

// Initialize segmentation. Create a GDT and load it in GDTR.
void Init();

// Run segmentation tests.
void Test(SelfTests::TestRunner& runner);

// Configure the current cpu to use the kernel-wide GDT allocated during Init().
// This both configures the GDTR and updates all segment registers to use the
// new segments.
void switchToKernelGdt();

}
