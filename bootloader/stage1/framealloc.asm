; Routines for allocating physical frames.

%include "macros.mac"

; EBDA is right below 0x000A0000 and is "absolutely guaranteed" to be at most
; 128KiB in size, hence starting at a minimum @0x00080000. While there are ways
; to get the exact size of the EBDA, assuming it starts @0x00080000 is simpler
; for now.
EBDA_START  EQU 0x00080000

; Size of physical frame in x86.
FRAME_SIZE  EQU 0x1000

SECTION .data
; The next free frame to be allocated by allocFrameLowMem. On boot this starts
; at the frame immediately preceeding the EBDA. Each new allocation allocates
; the frame preceeding the last allocated frame.
nextFrameLowMem:
DD  (EBDA_START - FRAME_SIZE)

SECTION .text

BITS    32

; ==============================================================================
; Allocate a physical frame in low memory (<1MiB). Frames are allocated in the
; Conventional Memory range STAGE_1_END-0x0007FFFF, right below the EBDA. This
; region is only ~480.5KiB max so while this is not much this should be enough
; to setup a small ID mapping and jump to long mode. If no space is available,
; this function panics.
; Note: The frame is zero'ed upon allocation.
; @return: The physical offset of the allocated frame.
DEF_GLOBAL_FUNC(allocFrameLowMem):
    push    ebp
    mov     ebp, esp

    ; EAX = physical offset of allocated frame.
    mov     eax, [nextFrameLowMem]

    ; Update nextFrameLowMem pointer.
    sub     DWORD [nextFrameLowMem], FRAME_SIZE

    ; Check that we did not overlap with stage1's memory.
    EXTERN  STAGE_1_END
    lea     ecx, [STAGE_1_END]
    cmp     eax, ecx
    ja      .allocOk
    ; The allocation is overlapping stage1. Panic now, nothing good can come out
    ; of this.
    LOG     "No physical frame available"
.dead:
    hlt
    jmp     .dead

.allocOk:
    ; Zero the frame.
    push    eax
    push    edi

    mov     edi, eax
    xor     eax, eax
    mov     ecx, (FRAME_SIZE / 4)
    cld
    rep stosd

    pop     edi
    pop     eax

    leave
    ret
