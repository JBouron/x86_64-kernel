; Helper routines related to Protected-Mode.
%ifndef PM_INC
%define PM_INC

%include "macros.mac"

; Enable and jump into protected mode. This function DOES NOT RETURN but instead
; jump to the address specified as parameter. Upon jumping to the target 32-bit
; code, the state of the stack and of the callee-saved registers are the same as
; before the call to jumpToProtectedMode.
; @param (WORD) targetAddr: The 32-bit code to jump to after protected mode has
; been enabled.
EXT_FUNC(jumpToProtectedMode)

; The saved real-mode IDTR as it was before jumping to protected mode. One
; should be careful using this value, let alone modifying it. The only reason
; this is a global var is because the bioscall test needs it.
EXT_VAR(realModeIdtr)
; Shortcut to access the base of the saved real-mode IDTR.
EXT_VAR(realModeIdtrBase)

%endif
; vim:syntax=asm filetype=asm
