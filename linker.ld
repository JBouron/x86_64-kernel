ENTRY(kernelEntry)

SECTIONS {
    /* Link kernel above the last 2GB of virtual address space. */
    . = 0xffffffff80000000;
    KERNEL_START_VADDR = .;

    .text BLOCK(4K) : ALIGN(4K) {
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(.bss)
    }
    KERNEL_END_VADDR = .;
}
