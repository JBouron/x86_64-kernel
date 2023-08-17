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

; Load an IDT using the LIDT instruction. Implemented in assembly.
; @param desc: Pointer to Table descriptor for the IDT to be loaded.
; extern "C" void _lidt(TableDesc const * const desc);
GLOBAL  _lidt:function
_lidt:
    push    rbp
    mov     rbp, rsp

    lidt    [rdi]

    leave
    ret

; Read the current value loaded in IDTR. Implemented in assembly.
; @param destBase: Where the base of the IDT should be stored.
; @param destLimit: Where the limit of the IDT should be stored.
; extern "C" void _sidt(u64 * const destBase, u16 * const destLimit);
GLOBAL  _sidt:function
_sidt:
    push    rbp
    mov     rbp, rsp

    ; Make some space to read the IDTR onto the stack.
    sub     rsp, 10
    sidt    [rsp]

    ; Write limit.
    mov     ax, [rsp]
    mov     [rsi], ax

    ; Write base.
    mov     rax, [rsp + 2]
    mov     [rdi], rax

    leave
    ret

; Set the segment reg Xs to the value sel. Implemented in assembly.
; @param sel: The new value for Xs.
; extern "C" void _setCs(u16 const sel);
; extern "C" void _setDs(u16 const sel);
; extern "C" void _setEs(u16 const sel);
; extern "C" void _setFs(u16 const sel);
; extern "C" void _setGs(u16 const sel);
; extern "C" void _setSs(u16 const sel);
%macro _setSReg 2
    GLOBAL %1:function
    %1:
        mov     %2, di
        ret
%endmacro

_setSReg _setDs, ds
_setSReg _setEs, es
_setSReg _setFs, fs
_setSReg _setGs, gs
_setSReg _setSs, ss

; For _setCs the procedure is a bit more involved since we need a long jump.
GLOBAL  _setCs:function
_setCs:
    ; Allocate a far pointer on the stack.
    sub     rsp, 10
    mov     [rsp + 0x8], di
    mov     QWORD [rsp], .longJumpTarget
    jmp     FAR QWORD [rsp]
.longJumpTarget:
    ; De-alloc the far pointer.
    add     rsp, 10
    ret

; Get the current value of the segment reg Xs. Implemented in assembly.
; extern "C" u16 _getCs();
; extern "C" u16 _getDs();
; extern "C" u16 _getEs();
; extern "C" u16 _getFs();
; extern "C" u16 _getGs();
; extern "C" u16 _getSs();
%macro _getSReg 2
    GLOBAL %1:function
    %1:
        mov     ax, %2
        movzx   eax, ax
        ret
%endmacro

_getSReg _getCs, cs
_getSReg _getDs, ds
_getSReg _getEs, es
_getSReg _getFs, fs
_getSReg _getGs, gs
_getSReg _getSs, ss

GLOBAL  _readCr0:function
_readCr0:
    mov     rax, cr0
    ret

GLOBAL  _writeCr0:function
_writeCr0:
    mov     cr0, rdi
    ret

GLOBAL  _readCr2:function
_readCr2:
    mov     rax, cr2
    ret

GLOBAL  _readCr3:function
_readCr3:
    mov     rax, cr3
    ret

GLOBAL  _writeCr3:function
_writeCr3:
    mov     cr3, rdi
    ret

; Implementation of outb() in assembly.
; @param port: The port to output into.
; @param value: The byte to write to the port.
; extern "C" void _outb(u16 const port, u8 const value);
GLOBAL  _outb:function
_outb:
    mov     dx, di
    mov     al, sil
    out     dx, al
    ret

;  Implementation of inb() in assembly.
;  @param port: The port to read from.
;  @return: The byte read from the port.
; extern "C" u8 _inb(u16 const port);
GLOBAL  _inb:function
_inb:
    mov     dx, di
    in      al, dx
    movzx   eax, al
    ret
