; Memory allocation routines.
%include "macros.mac"

; ==============================================================================
; Allocate memory.
; @param (DWORD) size: The number of bytes to allocate.
; @return (EAX): The address of the freshly allocated memory. This memory is
; zeroed upon allocation.
DEF_GLOBAL_FUNC64(malloc):
    push    rbp
    mov     rbp, rsp

    ; RCX = Size of allocation in bytes.
    mov     rcx, rdi

    ; RDX = Address of allocated memory.
    mov     rdx, [heapTop]
    add     rdx, rcx

    ; Save the new heapTop.
    mov     [heapTop], rdx

    ; Zero the allocated mem. RCX already contains the number of bytes to zero
    ; at this point.
    mov     rdi, rdx
    xor     al, al
    cld
    rep stosb

    mov     rax, rdx

    leave
    ret

; Pointer to the top of the heap.
; On x86 the memory region 0x00000500-0x00007BFF is usable memory. The stage 1
; stack has been setup to start at 0x7c00 and grow down towards 0x500. The heap
; therefore naturally starts at offset 0x500 and grow up-wards.
DEF_LOCAL_VAR(heapTop):
DQ  0x500
