; This file defines the interrupt handlers for each vector.
; The goal here is perform the necessary bookkeeping before jumping to C++ where
; the real business is done. Once the C++ handler return we IRET back to the
; interrupted context.

SECTION .text
BITS    64

; Interrupts are handled as follows:
; ----------------------------------
;   * There is one interrupt handler per vector (as expected by x86 arch).
;   * For vectors that do not push an error code, a dummy error code (0) is
;   pushed onto the stack.
;   * All interrupt handlers jump to a common handler, passing the vector as
;   argument (pushed on the stack).
; Therefore, upon jumping to the common handler, the stack looks like this:
;   |   Return SS   | +0x28
;   |   Return RSP  | +0x20
;   | Return RFLAGS | +0x18
;   |   Return CS   | +0x10
;   |   Return RIP  | +0x08
;   |   Error code  | <- RSP

; Define an interrupts handler for the given vector. The handler is named
; "interruptHandler<vector>". The handler first pushes a dummy error code (0),
; if the vector does not push an error code, followed by a push of the vector
; number before jumping to interruptHandlerCommon.
%macro INT_HANDLER 1
%if (%1 < 0) || (255 < %1)
    %error "Vector number is not between 0 and 255"
%endif
    ; Label definition. Must be declared global so that C++ knows the addresses
    ; to put in the the IDT entries.
    GLOBAL  %tok(%strcat("interruptHandler", %1)):function
    %tok(%strcat("interruptHandler", %1)):
%if (%1 == 8) || ((10 <= %1) && (%1 <= 14)) || (%1 == 17) || (%1 == 21)
    ; The vector pushes an error code on the stack, nothing to do.
%else
    ; The vector does not push an error code on the stack, push a zero.
    push    0x0
%endif
    ; Push the vector number onto the stack.
    push    %1
    ; Jump to the common handler.
    jmp     interruptHandlerCommon
%endmacro

; All interrupt handlers, one per vector.
INT_HANDLER 0
INT_HANDLER 1
INT_HANDLER 2
INT_HANDLER 3
INT_HANDLER 4
INT_HANDLER 5
INT_HANDLER 6
INT_HANDLER 7
INT_HANDLER 8
INT_HANDLER 10
INT_HANDLER 11
INT_HANDLER 12
INT_HANDLER 13
INT_HANDLER 14
INT_HANDLER 16
INT_HANDLER 17
INT_HANDLER 18
INT_HANDLER 19
INT_HANDLER 21
; User-defined vectors.
INT_HANDLER 32
INT_HANDLER 33
INT_HANDLER 34
INT_HANDLER 35

; The common interrupt handler. All per-vector handlers are jumping to this
; routine after pushing the vector onto the stack.
interruptHandlerCommon:
    ; Save all the GP registers onto the stack.
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rdi
    push    rsi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    ; RAX = Pointer to interrupt frame pushed by the CPU including the error
    ; code (or the dummy one pushed by the handler).
    lea     rax, [rsp + 16 * 0x8]
    ; For RSP, push the "return RSP", e.g. the value of the RSP before the
    ; interrupt. It can be found in the interrupt frame.
    push    QWORD [rax + 4 * 0x8]
    ; RFLAGS.
    push    QWORD [rax + 3 * 0x8]
    ; RIP.
    push    QWORD [rax + 1 * 0x8]
    ; Error code.
    push    QWORD [rax]

    ; Call the generic interrupt handler, with the vector number as param.
    EXTERN  genericInterruptHandler 
    ; Pass the vector number to the genericInterruptHandler.
    mov     rdi, [rax - 0x8]
    lea     rsi, [rsp]
    call    genericInterruptHandler

    ; Remove the frame info from the stack, e.g. error code, RIP, RFLAGS and
    ; RSP.
    add     rsp, 4 * 0x8

    ; Restore the registers of the interrupted context.
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rsi
    pop     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Pop the vector and (possibly dummy) error code from the stack.
    add     rsp, 0x10

    ; Return to the interrupted context.
    iretq

