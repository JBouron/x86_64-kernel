SECTION .text
BITS    64

EXTERN  contextSwitch
EXTERN  contextSwitchTestSavedOrigRsp
EXTERN  contextSwitchTestSavedContextRsp
EXTERN  contextSwitchTestTargetFlag

; extern "C" void contextSwitchTestTarget()
GLOBAL  contextSwitchTestTarget:function
contextSwitchTestTarget:
    ; Clobber all callee-saved registers.
    not     rbx
    not     rbp
    not     r12
    not     r13
    not     r14
    not     r15

    ; Set the flag.
    mov     BYTE [contextSwitchTestTargetFlag], 1

    ; Switch to the previous context.
    mov     rdi, [contextSwitchTestSavedOrigRsp]
    lea     rsi, [contextSwitchTestSavedContextRsp]
    call    contextSwitch

    ; We should never return from the call to contextSwitch in this test.
    int3


; extern "C" bool initiateContextSwitchAndCheckCalleeSavedRegs(
;    u64 const newRsp, u64* const savedRsp);
GLOBAL  initiateContextSwitchAndCheckCalleeSavedRegs:function
initiateContextSwitchAndCheckCalleeSavedRegs:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rbx, 0xAAAAAAAAAAAAAAAA
    mov     rbp, 0xBBBBBBBBBBBBBBBB
    mov     r12, 0xCCCCCCCCCCCCCCCC
    mov     r13, 0xDDDDDDDDDDDDDDDD
    mov     r14, 0xEEEEEEEEEEEEEEEE
    mov     r15, 0xFFFFFFFFFFFFFFFF

    ; RDI and RSI still contain the original args.
    call    contextSwitch

    ; Compare the current callee-saved registers with the expected values.
    mov     rax, 0xAAAAAAAAAAAAAAAA
    cmp     rbx, rax
    jne     .fail
    mov     rax, 0xBBBBBBBBBBBBBBBB
    cmp     rbp, rax
    jne     .fail
    mov     rax, 0xCCCCCCCCCCCCCCCC
    cmp     r12, rax
    jne     .fail
    mov     rax, 0xDDDDDDDDDDDDDDDD
    cmp     r13, rax
    jne     .fail
    mov     rax, 0xEEEEEEEEEEEEEEEE
    cmp     r14, rax
    jne     .fail
    mov     rax, 0xFFFFFFFFFFFFFFFF
    cmp     r15, rax
    jne     .fail

    ; Success
    mov     rax, 1
    jmp     .out

.fail:
    xor     rax, rax
.out:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx

    ret
