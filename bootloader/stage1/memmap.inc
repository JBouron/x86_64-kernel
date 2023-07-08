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

NO_SUCH_FRAME   EQU -1
; ==============================================================================
; Find the first available physical page frame above a particular offset.
; @param %RDI: Address of the memory map.
; @param %RSI: Number of entries in the memory map.
; @param %RDX: Start offset of the search.
; @return (RAX): The physical offset of the first available frame above %RDX. If
; no such frame exists then NO_SUCH_FRAME is returned.
EXT_FUNC(findFirstAvailFrame)

%endif
; vim:syntax=asm filetype=asm
