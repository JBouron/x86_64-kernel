// Misc functions to directly interact with the CPU. Typically wrapper around
// specific instructions.

#pragma once
#include <util/util.hpp>
#include <selftests/selftests.hpp>

namespace Cpu {

// Run the tests under this namespace.
void Test(SelfTests::TestRunner& runner);

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

    // Read limit and base of a TableDesc.
    u16 limit() const { return m_limit; }
    u64 base() const { return m_base; }

private:
    u16 const m_limit;
    u64 const m_base;
} __attribute__((packed));
static_assert(sizeof(TableDesc) == 10);

// Load a GDT using the LGDT instruction.
// @param desc: Table descriptor for the GDT to be loaded.
void lgdt(TableDesc const& desc);

// Read the current value stored in the GDTR using the SGDT instruction.
// @return: The current value loaded in GDTR.
TableDesc sgdt();

// Load an IDT using the LIDT instruction.
// @param desc: Table descriptor for the IDT to be loaded.
void lidt(TableDesc const& desc);

// Read the current value stored in the IDTR using the SIDT instruction.
// @return: The current value loaded in IDTR.
TableDesc sidt();


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


// #############################################################################
// Control registers
// #############################################################################

// Read the current value of CR0.
u64 cr0();

// Write the CR0 register.
// @param value: The value to be written in the cr0 register.
void writeCr0(u64 const value);

// Read the current value of CR2.
u64 cr2();

// Read the current value of CR3.
u64 cr3();

// Write the CR3 register.
// @param value: The value to be written in the cr3 register.
void writeCr3(u64 const value);


// #############################################################################
// I/O instructions.
// #############################################################################

// An I/O port.
using Port = u16;

// Output a byte in an I/O port.
// @param port: The port to output into.
// @param value: The byte to write to the port.
void outb(Port const port, u8 const value);

// Output a word in an I/O port.
// @param port: The port to output into.
// @param value: The word to write to the port.
void outw(Port const port, u16 const value);

// Read a byte from an I/O port.
// @param port: The port to read from.
// @return: The byte read from the port.
u8 inb(Port const port);


// #############################################################################
// CPUID
// #############################################################################

// Store the result of a cpuid() call, that is the value of the EAX, EBX, ECX
// and EDX registers as they were right after executing the CPUID instruction.
struct CpuidResult {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

// Execute the CPUID instruction with the given param.
// @param eax: The value to set the EAX register to before executing the CPUID
// instruction.
// @return: A CpuidResult containing the output of CPUID.
CpuidResult cpuid(u32 const eax);


// #############################################################################
// MSRs
// #############################################################################

// MSR values. Use an enum so that we avoid making mistakes using raw u32s.
enum class Msr : u32 {
    IA32_APIC_BASE = 0x1b,
};

// Read a MSR.
// @param msr: The MSR to read.
// @return: The current value of the MSR.
u64 rdmsr(Msr const msr);

// Write into a MSR.
// @param msr: The MSR to write into.
// @param value: The value to write into the MSR.
void wrmsr(Msr const msr, u64 const value);

// Read the Time-Stamp Counter.
// @return: The current value of the TSC.
u64 rdtsc();

// Disable external interrupts on the CPU.
void disableInterrupts();
// Enable external interrupts on the CPU.
void enableInterrupts();
// Set the value of the Interrupt Flag (IF) in RFLAGS.
void setInterruptFlag(bool const ifValue);
// Check if the interrupts are enabled in the RFLAGS of this cpu.
// @return: true if the interrupts are enabled, false otherwise.
bool interruptsEnabled();

// Get the value of the RSP register.
// @return: The value of RSP when the call instruction jumping to this function
// was executed.
extern "C" u64 getRsp();

}
