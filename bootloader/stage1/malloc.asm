; Memory allocation routines.
%include "macros.mac"

BITS    32

; ==============================================================================
; Allocate memory.
; @param (DWORD) size: The number of bytes to allocate.
; @return (EAX): The address of the freshly allocated memory. This memory is
; zeroed upon allocation.
DEF_GLOBAL_FUNC(malloc):
    push    ebp
    mov     ebp, esp

    ; ECX = Size of allocation in bytes.
    mov     ecx, [ebp + 0x8]

    ; EDX = Address of allocated memory.
    mov     edx, [heapTop]
    add     edx, ecx

    ; Save the new heapTop.
    mov     [heapTop], edx

    ; Zero the allocated mem. ECX already contains the number of bytes to zero
    ; at this point.
    mov     edi, edx
    xor     al, al
    cld
    rep stosb

    mov     eax, edx

    leave
    ret

SECTION .data
; Pointer to the top of the heap.
; On x86 the memory region 0x00000500-0x00007BFF is usable memory. The stage 1
; stack has been setup to start at 0x7c00 and grow down towards 0x500. The heap
; therefore naturally starts at offset 0x500 and grow up-wards.
heapTop:
DW  0x500
