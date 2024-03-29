# x86_64-kernel

A bootloader and kernel for the x86_64 architecture, written in x86 assembly and
C++ respectively.

The two-stage bootloader is 100% hand-written x86 assembly. As of now it is
capable of:
- Initializing the CPU from real-mode all the way to 64-bit mode.
- Parse the E820 memory map from the BIOS.
- Read the kernel ELF image from the boot disk (the ELF immediately follows the
  bootloader's second stage on disk, i.e. no partitioning and no filesystem
  support).
- Parse the kernel ELF and load its segments to memory.
- Jump to the 64-bit kernel entry point.

The C++ kernel is still **work in progress**. As of now the following are
supported:
- Basic CPU initialization: segmentation, interrupts, paging
- ACPI table parsing
- IO-APIC(s)
- LAPIC
- Multicore support, including Inter-Processor-Interrupts
- Processes and context switch, although the scheduler is not yet implemented.
