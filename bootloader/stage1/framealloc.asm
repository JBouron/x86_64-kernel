; Routines for allocating physical frames.

%include "macros.mac"
%include "memmap.inc"

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
    PANIC   "No physical frame available"

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

SECTION .data

; Controls the minimum offset to be returned by allocFrame. We try to avoid
; touching anything under 1MiB as the granularity of the E820 memory map under
; 1MiB is not that great and we run the risk of corrupting BIOS memory.
; Everything over 1MiB should be fair-game.
NEXT_FRAME_MIN_OFFSET   EQU 0x100000

; Physical pointer to the next physical frame that is available for allocation.
; Initialized to the first available frame above NEXT_FRAME_MIN_OFFSET.
nextFrame:
DQ  0x0

SECTION .text
BITS    64

; ==============================================================================
; Allocate a frame in available memory. This function uses the memory map to
; find available frames and as such cannot be called prior to calling
; parseMemoryMap.
; @return (RAX): The physical address of the allocated frame.
DEF_GLOBAL_FUNC(allocFrame):
    push    rbp
    mov     rbp, rsp

    ; Check that the memory map has been parsed from E820.
    mov     rax, [memoryMap]
    test    rax, rax
    jnz     .memoryMapOk
    ; The memory map was not yet parsed, we cannot know which memory is safe to
    ; be allocated!
    PANIC   "Attempt to call allocFrame before calling parseMemoryMap"
.memoryMapOk:

    ; On the first call to allocFrame, initialize the nextFrame pointer.
    mov     rax, [nextFrame]
    test    rax, rax
    jnz     .nextFrameInitialized

    DEBUG   "First call to allocFrame, initializating nextFrame pointer"
    mov     rdi, [memoryMap]
    mov     rsi, [memoryMapLen]
    mov     rdx, NEXT_FRAME_MIN_OFFSET
    call    findFirstAvailFrame
    cmp     rax, -1
    jne     .nextFrameProceedInit
    mov     rax, NEXT_FRAME_MIN_OFFSET
    PANIC   "Failed to initialize allocFrame: No free frame above $", rax
.nextFrameProceedInit:
    mov     [nextFrame], rax
    DEBUG   "allocFrame.nextFrame initialized to $", rax
.nextFrameInitialized:

    ; RDX = next available frame
    mov     rdx, [nextFrame]

    ; Check if we ran out of available frames.
    mov     rax, -1
    cmp     rax, rcx
    jne     .frameAvailable
    PANIC   "No free frame available"
.frameAvailable:

    ; Save next avail frame offset, this will be the return value.
    push    rdx

    ; Advance the nextFrame pointer.
    mov     rdi, [memoryMap]
    mov     rsi, [memoryMapLen]
    ; RDX is already set to nextFrame, we want to search for the next available
    ; frame starting at nextFrame + FRAME_SIZE.
    add     rdx, FRAME_SIZE
    call    findFirstAvailFrame
    mov     [nextFrame], rax

    ; Set return value to the previous nextFrame value.
    pop     rax

    leave
    ret
