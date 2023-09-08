; This file contains assembly code used by the SMP tests.

SECTION .text

; Used by wakeApplicationProcessorTest.
; This code starts with a 4-bytes NOP-slides to real-mode code that replaces
; this slide with a DWORD of value 0xb1e2b007.
BITS    16
GLOBAL  wakeApplicationProcessorTestBootCode:function
wakeApplicationProcessorTestBootCode:
    ; Make space for the flag.
    nop
    nop
    nop
    nop
    ; The code is always stored at the beginning of a page (because of how AP
    ; startup works) and the CS:IP is set to vv00:0000 where vv is the vector
    ; in the SIPI. This means that the offset of the first byte of the NOP-slide
    ; above is _always_ 0x0 in the current code segment.
    ; Set DS = CS so that we can write the flag in the same segment we are
    ; executing on.
    mov     ax, cs
    mov     ds, ax
    ; Overwrite the NOP-slide with the flag.
    mov     eax, 0xb1e2b007
    xor     bx, bx
    mov     [bx], eax
    ; Halt this cpu.
.dead:
    cli
    hlt
    jmp     .dead
GLOBAL  wakeApplicationProcessorTestBootCodeEnd:function
wakeApplicationProcessorTestBootCodeEnd:
