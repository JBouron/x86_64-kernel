; Assembly routines related to stack managment.

SECTION .text
BITS    64

; Change the stack pointer to the new top and jump to the given location. This
; function does not return.
; @param newStackTop: Virtual address of the new stack to use.
; @param jmpTarget: Destination of the jump after switching to the new stack.
GLOBAL  _switchToStackAndJumpTo:function
_switchToStackAndJumpTo:
    mov     rsp, rdi
    jmp     rsi
