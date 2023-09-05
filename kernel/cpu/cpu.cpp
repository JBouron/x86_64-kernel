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
    if (limit > 0 && limit % 8 != 7) {
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

// Load an IDT using the LIDT instruction. Implemented in assembly.
// @param desc: Pointer to Table descriptor for the IDT to be loaded.
extern "C" void _lidt(TableDesc const * const desc);

// Load an IDT using the LIDT instruction.
// @param desc: Table descriptor for the IDT to be loaded.
void lidt(TableDesc const& desc) {
    _lidt(&desc);
}

// Read the current value loaded in IDTR. Implemented in assembly.
// @param destBase: Where the base of the IDT should be stored.
// @param destLimit: Where the limit of the IDT should be stored.
extern "C" void _sidt(u64 * const destBase, u16 * const destLimit);

// Read the current value stored in the IDTR using the SIDT instruction.
// @return: The current value loaded in IDTR.
TableDesc sidt() {
    u64 base;
    u16 limit;
    _sidt(&base, &limit);
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

// Create a segment selector value referring to the GDT.
// @param selectorIndex: The index of the segment to point to in the GDT.
// @param rpl: The requested privilege level.
SegmentSel::SegmentSel(u16 const selectorIndex, PrivLevel const rpl) :
    SegmentSel(selectorIndex, false, rpl) {}

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
    UNREACHABLE
}

// Implementation of cr0() in assembly.
extern "C" u64 _readCr0();

// Read the current value of CR0.
u64 cr0() {
    return _readCr0();
}

// Implementation of writeCt0() in assembly.
extern "C" void _writeCr0(u64);

// Write the CR0 register.
// @param value: The value to be written in the cr0 register.
void writeCr0(u64 const value) {
    _writeCr0(value);
}

// Implementation of cr2() in assembly.
extern "C" u64 _readCr2();

// Read the current value of CR2.
u64 cr2() {
    return _readCr2();
}

// Implementation of cr3() in assembly.
extern "C" u64 _readCr3();

// Read the current value of CR3.
u64 cr3() {
    return _readCr3();
}

// Implementation of writeCt3() in assembly.
extern "C" void _writeCr3(u64);

// Write the CR3 register.
// @param value: The value to be written in the cr3 register.
void writeCr3(u64 const value) {
    _writeCr3(value);
}


// Implementation of outb() in assembly.
// @param port: The port to output into.
// @param value: The byte to write to the port.
extern "C" void _outb(u32 const port, u8 const value);

// Output a byte in an I/O port.
// @param port: The port to output into.
// @param value: The byte to write to the port.
void outb(Port const port, u8 const value) {
    _outb(port, value);
}

// Implementation of inb() in assembly.
// @param port: The port to read from.
// @return: The byte read from the port.
extern "C" u8 _inb(u32 const port);

// Read a byte from an I/O port.
// @param port: The port to read from.
// @return: The byte read from the port.
u8 inb(Port const port) {
    return _inb(port);
}

// Execute the CPUID instruction with the given parameters. Implemented in
// assembly.
// @param inEax: The value to set the EAX register to before executing CPUID.
// @param inEcx: The value to set the ECX register to before executing CPUID.
// @param outEax: The value of the EAX register as it was after executing CPUID.
// @param outEbx: The value of the EBX register as it was after executing CPUID.
// @param outEcx: The value of the EBX register as it was after executing CPUID.
// @param outEdx: The value of the EBX register as it was after executing CPUID.
extern "C" void _cpuid(u32 const inEax,
                       u32 const inEcx,
                       u32* const outEax,
                       u32* const outEbx,
                       u32* const outEcx,
                       u32* const outEdx);

// Execute the CPUID instruction with the given param.
// @param eax: The value to set the EAX register to before executing the CPUID
// instruction.
// @return: A CpuidResult containing the output of CPUID.
CpuidResult cpuid(u32 const eax) {
    CpuidResult res;
    // For now we don't support passing an arg to CPUID through ECX as this is
    // rarely needed. _cpuid does support however, so if we ever need this this
    // is only a matter of updating Cpu::cpuid()'s prototype to include ecx.
    _cpuid(eax, 0x0, &res.eax, &res.ebx, &res.ecx, &res.edx);
    return res;
}

// Implementation of rdmsr, written in assembly.
extern "C" u64 _rdmsr(u32 const msr);

// Read a MSR.
// @param msr: The MSR to read.
// @return: The current value of the MSR.
u64 rdmsr(Msr const msr) {
    return _rdmsr(static_cast<u32>(msr));
}

// Implementation of wrmsr, written in assembly.
extern "C" void _wrmsr(u32 const msr, u64 const value);

// Write into a MSR.
// @param msr: The MSR to write into.
// @param value: The value to write into the MSR.
void wrmsr(Msr const msr, u64 const value) {
    _wrmsr(static_cast<u32>(msr), value);
}

// Implementation of rdtsc(), in assembly.
extern "C" u64 _rdtsc();

// Read the Time-Stamp Counter.
// @return: The current value of the TSC.
u64 rdtsc() {
    return _rdtsc();
}

// Disable external interrupts on the CPU.
void disableInterrupts() {
    asm("cli");
}

// Enable external interrupts on the CPU.
void enableInterrupts() {
    asm("sti");
}

// Set the value of the Interrupt Flag (IF) in RFLAGS.
void setInterruptFlag(bool const ifValue) {
    if (ifValue) {
        enableInterrupts();
    } else {
        disableInterrupts();
    }
}
}
