; Dead-simple kernel stub to exercise ELF loading capabilities of the
; bootloader, simply prints hello world in the VGA buffer.
; We try to have separate .text and .data sections. This stubs reads a variable
; from the .data section into RAX and then just halts forever.
BITS    64

SECTION .text

GLOBAL  _start
_start:
    ; Quick and dirty string printing routine before halting forever.
    ; Print to the very last row of the VGA buffer.
    lea     rdi, [0xb8000 + 24 * 80 * 2]
    mov     rsi, [myStringPtr]
.loop:
    mov     al, [rsi]
    test    al, al
    je      .loopOut

    ; Print next char.
    mov     ah, (1 << 4) | 14
    mov     [rdi], ax

    ; Advance read and write pointers.
    inc     rsi
    add     rdi, 2
    jmp     .loop
.loopOut:
    cli
.dead:
    hlt
    jmp     .dead


SECTION .data
myStringPtr:
DQ  myString

SECTION .bss
RESQ    100

SECTION .rodata
myString:
DB "Hello World from kernel.asm!", 0
