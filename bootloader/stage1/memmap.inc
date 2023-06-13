; Routines related to memory map parsing.
%ifndef MEMMAP_INC
%define MEMMAP_INC

; ==============================================================================
; Parse the memory map as reported by the BIOS function INT=15h AX=E820h.
EXT_FUNC(parseMemoryMap)

%endif
; vim:syntax=asm filetype=asm
