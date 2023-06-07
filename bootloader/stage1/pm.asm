; Helper routines related to Protected-Mode.
%include "macros.mac"

SECTION .text

BITS 16

; Mask of the Protected-Enable (PE) bit in cr0.
CR0_PE_BIT_MASK EQU 1

; ==============================================================================
; Load the required GDT for protected mode.
_loadProtectedModeGdt:
    push    bp
    mov     bp, sp

    ; Prepare the table descriptor on the stack.
    push    0x0
    lea     ax, [stage1Gdt]
    push    ax
    mov     ax, stage1GdtEnd - stage1Gdt - 1
    push    ax

    ; Load the GDT descriptor into GDTR.
    lgdt    [bp - 0x6]

    leave
    ret

; ==============================================================================
; Enable and jump into protected mode. This function DOES NOT RETURN but instead
; jump to the address specified as parameter. Upon jumping to the target 32-bit
; code, the state of the stack and of the callee-saved registers are the same as
; before the call to jumpToProtectedMode.
; @param (WORD) targetAddr: The 32-bit code to jump to after protected mode has
; been enabled.
DEF_GLOBAL_FUNC(jumpToProtectedMode):
    ; Save the real mode IDTR so that we can re-use it in the future if we need
    ; to jump back to real-mode and execute BIOS functions.
    sidt    [realModeIdtr]

    ; Zero-out the IDTR before proceeding to protected-mode as it won't be
    ; compatible.
    push    bx
    xor     ax, ax
    push    ax
    push    ax
    push    ax
    mov     bx, sp
    lidt    [bx]
    add     sp, 0x6
    pop     bx

    ; Load the GDT to be used for protected mode operation.
    call    _loadProtectedModeGdt

    ; Enable protection enable (PE) bit in CR0
    mov     eax, cr0
    or      eax, CR0_PE_BIT_MASK
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

    ; Jump to the target address.
    movzx   eax, WORD [esp + 0x2]
    jmp     eax


SECTION .data

; Saved content of the IDTR as it was in real-mode before jumping to protected
; mode. When we need to go back to real-mode to call BIOS function we can reuse
; its value.
DEF_GLOBAL_VAR(realModeIdtr):
realModeIdtrLimit:
DW  0x0
DEF_GLOBAL_VAR(realModeIdtrBase):
; Technically 24-bits.
DD  0x0

; Declare a QWORD GDT entry with the given attributes. The resulting entry
; always P=1 (e.g. entry present), S=1 (e.g.  this is a code or data segment)
; DPL=0 (e.g. ring 0) and L=0 (e.g. non-64 bits).
; @param %1 base: The base of the segment.
; @param %2 limit: The limit of the segment in number of pages.
; @param %3 type: The type of the segment, see Table 3-1 in chapter 3.4.5.1 in
; Intel's manual.
; @param %4 db: The D/B bit.
; @param %5 gran: The G bit.
%macro GDT_ENTRY 5 ; (base, limit, type, db, gran)
    DD  (%1 & 0xffff) << 16 | (%2 & 0xffff)
    DD  (%1 & 0xff000000) | (%5 << 23) | (%4 << 22) | (%2 & 0xf0000) | \
        (1 << 15) | (1 << 12) | (%3 << 8) | ((%1 & 0xff0000) >> 16)
%endmacro

; The Global-Descriptor-Table used for stage 1. For now this only contains 32
; bit segments to be used in Protected Mode.
stage1Gdt:
; NULL entry
DQ  0x0
; 32-bit code segment, readable and executable.
GDT_ENTRY 0x0, 0xfffff, 10, 1, 1
; 32-bit data segment, readable and writable.
GDT_ENTRY 0x0, 0xfffff, 2, 1, 1
; 16-bit code segment, readable and executable. Used when jumping back to
; real-mode from 32-bit protected-mode.
GDT_ENTRY 0x0, 0xffff, 10, 0, 0
; 16-bit data segment, readable and writable. Used when jumping back to
; real-mode from 32-bit protected-mode.
GDT_ENTRY 0x0, 0xffff, 2, 0, 0
; This label is used to compute the size of the GDT when loading it, it must
; immediately follow the last entry.
stage1GdtEnd:
