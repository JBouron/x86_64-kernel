; Dead-simple kernel stub to exercise ELF loading capabilities of the
; bootloader.
; We try to have separate .text and .data sections. This stubs reads a variable
; from the .data section into RAX and then just halts forever.
BITS    64

SECTION .text

GLOBAL  _start
_start:
    mov     rax, [var]
    cli
.dead:
    hlt
    jmp     .dead


SECTION .data
var:
DQ  0xdeadcafebeefbabe
