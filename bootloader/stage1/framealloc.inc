; Routines for allocating physical frames.

%ifndef FRAMEALLOC_INC
%define FRAMEALLOC_INC

; ==============================================================================
; Allocate a physical frame in low memory (<1MiB). Frames are allocated in the
; Conventional Memory range STAGE_1_END-0x0007FFFF, right below the EBDA. This
; region is only ~480.5KiB max so while this is not much this should be enough
; to setup a small ID mapping and jump to long mode. If no space is available,
; this function panics.
; Note: The frame is zero'ed upon allocation.
; @return: The physical offset of the allocated frame.
EXT_FUNC(allocFrameLowMem)

; ==============================================================================
; Allocate a frame in available memory. This function uses the memory map to
; find available frames and as such cannot be called prior to calling
; parseMemoryMap.
; @return (RAX): The physical address of the allocated frame.
EXT_FUNC(allocFrame)

; ==============================================================================
; Pointer to head of the physical frame free list. Only to be used by
; bootstruct.asm.
EXT_VAR(freeListHead)

%endif
; vim:syntax=asm filetype=asm
