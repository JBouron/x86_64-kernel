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
    ; Get the IP.
    call    .local
.local:
    ; BX = IP of current instruction (e.g. pop).
    ; Important note: The CS might not be zero here, actually it is not
    ; specified what the CS is, hence this IP really is relative to the current
    ; CS.
    pop     bx
    ; The offset of the pop instruction above is exactly 7 bytes from the start
    ; of the code (e.g. from the first nop). Hence the offset of the boot
    ; code in the current segment is:
    sub     bx, 7
    ; Set DS = CS so that we can write the flag in the same segment we are
    ; executing on.
    mov     ax, cs
    mov     ds, ax
    ; Overwrite the NOP-slide with the flag.
    mov     eax, 0xb1e2b007
    mov     [bx], eax
    ; Halt this cpu.
.dead:
    cli
    hlt
    jmp     .dead
GLOBAL  wakeApplicationProcessorTestBootCodeEnd:function
wakeApplicationProcessorTestBootCodeEnd:
