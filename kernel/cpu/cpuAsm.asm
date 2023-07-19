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

; Read the current value loaded in GDTR. Implemented in assembly.
; @param destBase: Where the base of the GDT should be stored.
; @param destLimit: Where the limit of the GDT should be stored.
; extern "C" void _sgdt(u64 * const destBase, u16 * const destLimit);
GLOBAL  _sgdt:function
_sgdt:
    push    rbp
    mov     rbp, rsp

    ; Read the GDTR onto the stack before writing it to the dest pointers.
    sub     rsp, 10
    sgdt    [rsp]

    ; Write limit.
    mov     ax, [rsp]
    mov     [rsi], ax

    ; Write base.
    mov     rax, [rsp + 2]
    mov     [rdi], rax

    leave
    ret
