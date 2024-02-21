; Low-level scheduling routines.
SECTION .text
BITS    64

; extern "C" void contextSwitch(u64 const rsp, u64* const rspSave);
GLOBAL  contextSwitch:function
contextSwitch:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save the current RSP.
    mov     [rsi], rsp

    ; Switch to the other stack.
    mov     rsp, rdi

    ; Restore the callee-saved registers.
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx

    ret
