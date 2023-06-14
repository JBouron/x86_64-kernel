; Helper routines to perform (some) arithmetic operations on 64-bit values while
; still running in 32-bit protected mode.

BITS    32

%include "macros.mac"

; ==============================================================================
; Add two unsigned 64-bit values.
; @param (DWORD): Pointer to the destination 64-bit value.
; @param (DWORD): Pointer to the first operand.
; @param (DWORD): Pointer to the second operand.
DEF_GLOBAL_FUNC(add64):
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 0xc]
    mov     ecx, [ebp + 0x10]

    mov     edx, [eax]
    add     edx, [ecx]
    push    edx

    mov     edx, [eax + 0x4]
    adc     edx, [ecx + 0x4]

    mov     eax, [ebp + 0x8]
    mov     [eax + 0x4], edx
    pop     DWORD [eax]

    leave
    ret

; ==============================================================================
; Subtract two unsigned 64-bit values.
; @param (DWORD): Pointer to the destination 64-bit value.
; @param (DWORD): Pointer to the first operand, e.g. the operand we are
; subtracting from.
; @param (DWORD): Pointer to the second operand, e.g. the value being subtracted
; from the other operand.
DEF_GLOBAL_FUNC(sub64):
    push    ebp
    mov     ebp, esp

    mov     eax, [ebp + 0xc]
    mov     ecx, [ebp + 0x10]

    mov     edx, [eax]
    sub     edx, [ecx]
    push    edx

    mov     edx, [eax + 0x4]
    sbb     edx, [ecx + 0x4]

    mov     eax, [ebp + 0x8]
    mov     [eax + 0x4], edx
    pop     DWORD [eax]

    leave
    ret
