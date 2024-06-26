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
    ; Use a far return instead of a far jump here in case we are running on an
    ; AMD processor. It turns out, AMD processors only support far jump to
    ; 32bit offsets in 64-bit mode and that offset is ZERO extended to 64-bit.
    ; Since we are in higher half, this cannot work (sign-extended would have
    ; worked). Our only option is therefore to use a RETF, e.g. far return.
    ; Although not mentioned in the docs, it seems that RETF is capable of
    ; returning to a far pointer of the form m16:64 (e.g. with 64-bit offset).
    ; My guess is this is because a far call can be done from any canonical
    ; 64-bit offset and as such the RETF must support jumping back to a 64-bit
    ; offset.
    ; Intel CPUs do not have this issue since they support far jump to m16:64.
    ; But keep it simple and use the RETF in both cases.
    ; For both Intel and AMD it seems that a RETF pops 16 bytes from the stack,
    ; e.g. 8-bytes for the offset + 8-bytes for the segment selector. This is
    ; not very clear in the docs, so just to be save the segment selector is
    ; zero-extended before being pushed on the stack.
    movzx   rdi, di
    push    rdi
    lea     rax, [.farJumpTarget]
    push    rax
    retfq
.farJumpTarget:
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

; Implementation of outw() in assembly.
; @param port: The port to output into.
; @param value: The word to write to the port.
; extern "C" void _outw(u16 const port, u16 const value);
GLOBAL  _outw:function
_outw:
    mov     dx, di
    mov     ax, si
    out     dx, ax
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

; Execute the CPUID instruction with the given parameters. Implemented in
; assembly.
; @param inEax: The value to set the EAX register to before executing CPUID.
; @param inEcx: The value to set the ECX register to before executing CPUID.
; @param outEax: The value of the EAX register as it was after executing CPUID.
; @param outEbx: The value of the EBX register as it was after executing CPUID.
; @param outEcx: The value of the EBX register as it was after executing CPUID.
; @param outEdx: The value of the EBX register as it was after executing CPUID.
; extern "C" void _cpuid(u32 const inEax,
;                        u32 const inEcx,
;                        u32* const outEax,
;                        u32* const outEbx,
;                        u32* const outEcx,
;                        u32* const outEdx);
GLOBAL  _cpuid:function
_cpuid:
    push    rbp
    mov     rbp, rsp

    push    rbx

    ; CPUID is going to clobber RDX and RCX which are currently holding the
    ; outEax and outEbx pointers respectively. Move them to R10 and R11.
    mov     r10, rdx
    mov     r11, rcx

    mov     eax, edi
    mov     ecx, esi
    cpuid

    ; At this point we have the following pointers:
    ;   R10 = outEax
    ;   R11 = outEbx
    ;   R9  = outEdx
    ;   R8  = outEcx
    mov     [r8], ecx
    mov     [r9], edx
    mov     [r10], eax
    mov     [r11], ebx

    pop     rbx
    leave
    ret

; Implementation of rdmsr, written in assembly.
; extern "C" u64 _rdmsr(u32 const msr);
GLOBAL  _rdmsr:function
_rdmsr:
    mov     ecx, edi
    rdmsr
    ; In x86_64 rdmsr clears the top halves of rdx and rax.
    shl     rdx, 32
    or      rax, rdx
    ret

; Implementation of wrmsr, written in assembly.
; extern "C" void _wrmsr(u32 const msr, u64 const value);
GLOBAL  _wrmsr:function
_wrmsr:
    mov     ecx, edi
    mov     eax, esi
    mov     rdx, rsi
    shr     rdx, 32

    wrmsr
    ret

; Implementation of rdtsc(), in assembly.
; extern "C" u64 _rdtsc();
GLOBAL  _rdtsc:function
_rdtsc:
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret

; extern "C" u64 _readRflags();
GLOBAL  _readRflags:function
_readRflags:
    pushfq
    pop     rax
    ret

; extern "C" u64 rsp()
GLOBAL  getRsp:function
getRsp:
    mov     rax, rsp
    add     rax, 8
    ret
