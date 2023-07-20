; This file defines the interrupt handlers for each vector.
; The goal here is perform the necessary bookkeeping before jumping back to C++
; where the real business is done. Once the C++ handler return we IRET back to
; the interrupted context.

SECTION .text
BITS    64

; x86_64 is a bit annoying due to the fact that the interrupt vector is not
; pushed onto the stack. Therefore the only way to differentiate interrupt
; vectors is to have each of them use their own handler.
; The goal here is to have those per-vector handlers be as small as possible
; since it can be duplicated up to 255 times (once for each vector) and have all
; these per-vector handlers jump to a common handler, while passing along the
; information about the vector number.

; A per-vector handler therefore performs the following:
;   1. Push a bool onto the stack indicating if this interrupt pushed an error
;   code.
;   2. Push the vector number onto the stack.
;   3. Jump to the common interrupt handler.
; Therefore the IRET is done in the common interrupt handler, not the per-vector
; handler.

; Declare and define a per-vector handler. The resulting handler is named
; interruptHandler<vector> and defined as a global function.
; @param %1: The vector of the interrupt.
; @param %2: A "boolean" (0 or 1) indicate if this vector as an associated error
; code.
%macro INTERRUPT_HANDLER 2
%if (%1 < 0) || (255 < %1)
%error "Vector number is not between 0 and 255"
%elif (%2 != 0) && (%2 != 1)
%error "Boolean is neither 0 nor 1"
%else
GLOBAL  %tok(%strcat("interruptHandler", %1)):function
%tok(%strcat("interruptHandler", %1)):
    push    %2
    push    %1
    jmp     interruptHandlerCommon
%endif
%endmacro

INTERRUPT_HANDLER 0, 0
INTERRUPT_HANDLER 1, 0
INTERRUPT_HANDLER 2, 0
INTERRUPT_HANDLER 3, 0
INTERRUPT_HANDLER 4, 0
INTERRUPT_HANDLER 5, 0
INTERRUPT_HANDLER 6, 0
INTERRUPT_HANDLER 7, 0
INTERRUPT_HANDLER 8, 1
INTERRUPT_HANDLER 10, 1
INTERRUPT_HANDLER 11, 1
INTERRUPT_HANDLER 12, 1
INTERRUPT_HANDLER 13, 1
INTERRUPT_HANDLER 14, 1
INTERRUPT_HANDLER 16, 0
INTERRUPT_HANDLER 17, 1
INTERRUPT_HANDLER 18, 0
INTERRUPT_HANDLER 19, 0
INTERRUPT_HANDLER 21, 1

; The common interrupt handler. All per-vector handlers are jumping to this
; routine after pushing the vector onto the stack.
; This routine is not declared global since it is of no use by the C++ code,
; unlike per-vector handlers which needs to be loaded by C++ into the IDT.
interruptHandlerCommon:
    ; Save all the GP registers onto the stack. Note: skip saving rsp, not
    ; needed.
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rdi
    push    rsi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Call the generic interrupt handler, with the vector number as param.
    EXTERN  genericInterruptHandler 
    mov     rdi, [rsp + 15 * 8]
    call    genericInterruptHandler

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rsi
    pop     rdi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Pop the vector number from the stack.
    add     rsp, 8

    ; Check if we need to pop the error code as well.
    cmp     QWORD [rsp], 1
    je      .popErrorCode
    ; No error code on the stack, pop the boolean.
    add     rsp, 8    
    jmp     .toIret
.popErrorCode:
    ; There is an error code on the stack, pop both the boolean and the error
    ; code at once.
    add     rsp, 16

.toIret:
    iretq

