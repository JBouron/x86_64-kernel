; Helper routines to perform (some) arithmetic operations on 64-bit values while
; still running in 32-bit protected mode.
%ifndef ARITH64_INC
%define ARITH64_INC

; ==============================================================================
; Add two unsigned 64-bit values.
; @param (DWORD): Pointer to the destination 64-bit value.
; @param (DWORD): Pointer to the first operand.
; @param (DWORD): Pointer to the second operand.
EXT_FUNC(add64)

; ==============================================================================
; Subtract two unsigned 64-bit values.
; @param (DWORD): Pointer to the destination 64-bit value.
; @param (DWORD): Pointer to the first operand, e.g. the operand we are
; subtracting from.
; @param (DWORD): Pointer to the second operand, e.g. the value being subtracted
; from the other operand.
EXT_FUNC(sub64)

; ==============================================================================
; Compare two unsigned 64-bit values.
; @param (DWORD): Pointer to the first operand.
; @param (DWORD): Pointer to the second operand.
; @return: EAX=-1 if the first operand is less than the second operand, EAX=0 if
; both operands are equals, EAX=1 if the first operand is bigger than the second
; operand.
EXT_FUNC(cmp64)

%endif
; vim:syntax=asm filetype=asm
