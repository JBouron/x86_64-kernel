; Routines related to memory map parsing.
%ifndef MEMMAP_INC
%define MEMMAP_INC

; Pointer to an array of Address Range Descriptor Structures (ARDS).
; Each ARDS has the following format:
;   {
;       u64 base;
;       u64 length; // in bytes.
;       u64 type; // 1 means available RAM, >= 2 means reserved.
;   }
EXT_VAR(memoryMap)
; The number of entry in the memoryMap array.
EXT_VAR(memoryMapLen)

; ==============================================================================
; Parse the memory map as reported by the BIOS function INT=15h AX=E820h.
; This initializes memoryMap and memoryMapLen.
EXT_FUNC(parseMemoryMap)

%endif
; vim:syntax=asm filetype=asm
