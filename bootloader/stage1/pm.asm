; Helper routines related to Protected-Mode.

BITS 16

; Mask of the Protected-Mode (PM) bit in cr0.
CR0_PM_BIT_MASK EQU 1

; ==============================================================================
; Load the required GDT for protected mode.
_loadProtectedModeGdt:
    push    bp
    mov     bp, sp

    ; Prepare the table descriptor on the stack.
    push    0x0
    lea     ax, [data.stage1Gdt]
    push    ax
    mov     ax, data.stage1GdtEnd - data.stage1Gdt - 1
    push    ax

    ; Load the GDT descriptor into GDTR.
    lgdt    [bp - 0x6]
    add     sp, 0x6

    leave
    ret

; Compute the value of a segment selector for GDT entry i.
%define SEG_SEL(i) (i << 3)

; ==============================================================================
; Enable and jump into protected mode. This function DOES NOT RETURN.
jumpToProtectedMode:
    ; Save the real mode IDTR so that we can re-use it in the future if we need
    ; to jump back to real-mode and execute BIOS functions.
    sidt    [data.realModeIdtr]

    ; Zero-out the IDTR before proceeding to protected-mode as it won't be
    ; compatible.
    xor     ax, ax
    push    ax
    push    ax
    push    ax
    ; We can clobber the BP register here since we don't use it and we won't
    ; return to the caller anyway.
    mov     bx, sp
    lidt    [bx]

    ; Load the GDT to be used for protected mode operation.
    call    _loadProtectedModeGdt

    ; Enable protected mode (PM) bit in CR0
    mov     eax, cr0
    or      eax, CR0_PM_BIT_MASK
    mov     cr0, eax

    ; Jump to the 32-bit code segment.
    jmp     SEG_SEL(1):.protectedModeTarget

BITS    32
.protectedModeTarget:
    ; We are now running in 32-bit protected mode.

    ; Set the other segment registers to use the 32-bit data segment.
    mov     ax, SEG_SEL(2)
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    call    clearVgaBuffer
    LOG     "Successfully jumped to protected mode"

    hlt
    jmp     .protectedModeTarget


; Fake data section
data:

; Saved content of the IDTR as it was in real-mode before jumping to protected
; mode. When we need to go back to real-mode to call BIOS function we can reuse
; its value.
.realModeIdtr:
.realModeIdtrLimit:
DW  0x0
.realModeIdtrBase:
; Technically 24-bits.
DD  0x0

; Declare a QWORD GDT entry with the given attributes. The resulting entry
; always use G=1 (e.g. page granularity), P=1 (e.g. entry present), S=1 (e.g.
; this is a code or data segment) DPL=0 (e.g. ring 0) and L=0 (e.g. non-64
; bits).
; @param %1 base: The base of the segment.
; @param %2 limit: The limit of the segment in number of pages.
; @param %3 type: The type of the segment, see Table 3-1 in chapter 3.4.5.1 in
; Intel's manual.
; @param %4 db: The D/B bit.
%macro GDT_ENTRY 4 ; (base, limit, type, db)
    DD  (%1 & 0xffff) << 16 | (%2 & 0xffff)
    DD  (%1 & 0xff000000) | (1 << 23) | (%4 << 22) | ((%2 & 0xf0000) << 16) | (1 << 15) | (1 << 12) | (%3 << 8) | ((%1 & 0xff0000) >> 16)
%endmacro

; The Global-Descriptor-Table used for stage 1. For now this only contains 32
; bit segments to be used in Protected Mode.
.stage1Gdt:
; NULL entry
DQ  0x0
; 32-bit code segment, readable and executable.
GDT_ENTRY 0x0, 0xfffff, 10, 1
; 32-bit data segment, readable and writable.
GDT_ENTRY 0x0, 0xfffff, 2, 1
; This label is used to compute the size of the GDT when loading it, it must
; immediately follow the last entry.
.stage1GdtEnd:
