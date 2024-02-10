; Assembly routines related to stack managment.

SECTION .text
BITS    64

; Change the stack pointer to the new top and jump to the given location. This
; function does not return.
; @param newStackTop: Virtual address of the new stack to use.
; @param jmpTarget: Destination of the jump after switching to the new stack.
; @param arg: Argument to pass to the jmpTarget function pointer.
GLOBAL  _switchToStackAndJumpTo:function
_switchToStackAndJumpTo:
    ; Switch to the new stack.
    mov     rsp, rdi
    ; Push a dummy return address. FIXME: We should push a function that leads
    ; to a useful PANIC.
    xor     rax, rax
    not     rax
    push    rax
    ; Setup the arg for the target function.
    mov     rdi, rdx
    ; Jump to the target function.
    jmp     rsi
