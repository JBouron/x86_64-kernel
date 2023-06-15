%ifndef LM_INC
%define LM_INC

%include "macros.mac"

; ==============================================================================
; Jump to 64-bit long mode. This function DOES NOT RETURN. The state of the
; stack on the target 64-bit code is as it was before pushing the parameter of
; this function.
; @param (DWORD): Target address. This is expected to point to 64-bit code.
EXT_FUNC(jumpToLongMode)

%endif
; vim:syntax=asm filetype=asm
