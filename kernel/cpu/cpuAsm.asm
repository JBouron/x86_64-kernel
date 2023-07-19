BITS    64
SECTION .text

; Load a GDT using the LGDT instruction. Implemented in assembly.
; @param desc: Pointer to Table descriptor for the GDT to be loaded.
; extern "C" void _lgdt(TableDesc const * const desc);
GLOBAL  _lgdt:function
_lgdt:
    push    rbp
    mov     rbp, rsp

    lgdt    [rdi]

    leave
    ret
