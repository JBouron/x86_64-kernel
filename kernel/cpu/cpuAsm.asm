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
