SECTION .text
BITS    64

EXTERN  interruptTestFlag

GLOBAL  interruptTestHandler0:function
interruptTestHandler0:
    mov     QWORD [interruptTestFlag], 0
    iretq

GLOBAL  interruptTestHandler1:function
interruptTestHandler1:
    mov     QWORD [interruptTestFlag], 1
    iretq

GLOBAL  interruptTestHandler3:function
interruptTestHandler3:
    mov     QWORD [interruptTestFlag], 3
    iretq

; extern "C" void clobberCallerSavedRegisters();
GLOBAL  clobberCallerSavedRegisters:function
clobberCallerSavedRegisters:
    ; Set all caller-saved regs to their 1-complements so that every single bit
    ; is changing.
    not     rax
    not     rcx
    not     rdx
    not     rsi
    not     rdi
    not     r8
    not     r9
    not     r10
    not     r11
    ret

; extern "C" bool interruptRegistersSavedTestRun();
GLOBAL  interruptRegistersSavedTestRun:function
interruptRegistersSavedTestRun:
    push    rbp
    mov     rbp, rsp

    ; We are going to clobber all registers in this test. Hence save the
    ; callee-saved regs.
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp

    ; Set all registers to some specific, but distinct, value.
    mov     rax, [EXP_RAX]
    mov     rbx, [EXP_RBX]
    mov     rcx, [EXP_RCX]
    mov     rdx, [EXP_RDX]
    mov     rsi, [EXP_RSI]
    mov     rdi, [EXP_RDI]
    mov     rbp, [EXP_RBP]
    mov     r8,  [EXP_R8]
    mov     r9,  [EXP_R9]
    mov     r10, [EXP_R10]
    mov     r11, [EXP_R11]
    mov     r12, [EXP_R12]
    mov     r13, [EXP_R13]
    mov     r14, [EXP_R14]
    mov     r15, [EXP_R15]
    ; Save the current value of RFLAGS.
    pushf

    ; Trigger the interrupt.
    int     1

    ; Save the current value of RFLAGS.
    pushf

    ; We first check that the RFLAGS did not change while executing the
    ; interrupt. To do so we need a free register, use RAX but check its value
    ; first.
    cmp     rax, [EXP_RAX]
    jne     .fail
    ; Now that rax is free check the before and after value of RFLAGS saved on
    ; the stack.
    mov     rax, [rsp]
    cmp     rax, [rsp + 0x8]
    jne     .fail

    ; Pop the saved RFLAGS from the stack. Not needed anymore.
    add     rsp, 0x10

    ; Check the remaining registers.
    cmp     rbx, [EXP_RBX]
    jne     .fail
    cmp     rcx, [EXP_RCX]
    jne     .fail
    cmp     rdx, [EXP_RDX]
    jne     .fail
    cmp     rsi, [EXP_RSI]
    jne     .fail
    cmp     rdi, [EXP_RDI]
    jne     .fail
    cmp     rbp, [EXP_RBP]
    jne     .fail
    cmp     r8,  [EXP_R8]
    jne     .fail
    cmp     r9,  [EXP_R9]
    jne     .fail
    cmp     r10, [EXP_R10]
    jne     .fail
    cmp     r11, [EXP_R11]
    jne     .fail
    cmp     r12, [EXP_R12]
    jne     .fail
    cmp     r13, [EXP_R13]
    jne     .fail
    cmp     r14, [EXP_R14]
    jne     .fail
    cmp     r15, [EXP_R15]
    jne     .fail

    ; All comparisons were succesful, test passed.
    jmp     .ok

.fail:
    xor     rax, rax
    jmp     .out
.ok:
    ; FIXME: Not sure how the ABI works with bools? I assume only setting AL to
    ; 1 should be sufficient here? Be safe just in case.
    mov     rax, 1
.out:
    ; Restore caller-saved registers.
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx

    leave
    ret

SECTION .rodata
; The expected values for the interruptRegistersSavedTestRun
; function.
EXP_RAX: DQ 0xaaaaaaaaaaaaaaaa
EXP_RBX: DQ 0xbbbbbbbbbbbbbbbb
EXP_RCX: DQ 0xcccccccccccccccc
EXP_RDX: DQ 0xdddddddddddddddd
EXP_RSI: DQ 0x1111111111111111
EXP_RDI: DQ 0x2222222222222222
EXP_RBP: DQ 0x3333333333333333
EXP_R8:  DQ 0x8888888888888888
EXP_R9:  DQ 0x9999999999999999
EXP_R10: DQ 0x1010101010101010
EXP_R11: DQ 0x1101101101101101
EXP_R12: DQ 0x1212121212121212
EXP_R13: DQ 0x1313131313131313
EXP_R14: DQ 0x1414141414141414
EXP_R15: DQ 0x1515151515151515
