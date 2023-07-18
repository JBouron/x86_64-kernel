; Routines for allocating physical frames.

%include "macros.mac"
%include "memmap.inc"
%include "malloc.inc"

; EBDA is right below 0x000A0000 and is "absolutely guaranteed" to be at most
; 128KiB in size, hence starting at a minimum @0x00080000. While there are ways
; to get the exact size of the EBDA, assuming it starts @0x00080000 is simpler
; for now.
EBDA_START  EQU 0x00080000

; Size of physical frame in x86.
FRAME_SIZE  EQU 0x1000

; The next free frame to be allocated by allocFrameLowMem. On boot this starts
; at the frame immediately preceeding the EBDA. Each new allocation allocates
; the frame preceeding the last allocated frame.
DEF_LOCAL_VAR(nextFrameLowMem):
DD  (EBDA_START - FRAME_SIZE)

; ==============================================================================
; Allocate a physical frame in low memory (<1MiB). Frames are allocated in the
; Conventional Memory range STAGE_1_END-0x0007FFFF, right below the EBDA. This
; region is only ~480.5KiB max so while this is not much this should be enough
; to setup a small ID mapping and jump to long mode. If no space is available,
; this function panics.
; Note: The frame is zero'ed upon allocation.
; @return: The physical offset of the allocated frame.
DEF_GLOBAL_FUNC32(allocFrameLowMem):
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

; Controls the minimum offset to be returned by allocFrame. We try to avoid
; touching anything under 1MiB as the granularity of the E820 memory map under
; 1MiB is not that great and we run the risk of corrupting BIOS memory.
; Everything over 1MiB should be fair-game.
NEXT_FRAME_MIN_OFFSET   EQU 0x100000

; Physical pointer to the next physical frame that is available for allocation.
; Initialized to the first available frame above NEXT_FRAME_MIN_OFFSET.
DEF_LOCAL_VAR(nextFrame):
DQ  0x0

; Frame allocator - Free list
; ===========================
; The frame allocator uses a typical non-embedded (e.g. stored in heap, not in
; free physical frame pages themselves) free list to maintain the list of
; physical page frames that are ready to be used/allocated.
; Each entry/node in the free list has the following layout:
FL_BASE         EQU 0x00 ; QWORD: Page aligned physical address of the first
                         ; free frame in this region.
FL_NUM_PAGES    EQU 0x08 ; QWORD: Number of pages available in this region.
FL_NEXT         EQU 0x10 ; QWORD: pointer to the next entry/node in the list.
FL_NODE_SIZE    EQU 0x18 ; Size of a node in bytes.

; Pointer to the head of the free list.
DEF_LOCAL_VAR(freeListHead):
DQ  0x0

; ==============================================================================
; Initialize the frame free-list.
DEF_LOCAL_FUNC64(initFreeList):
    push    rbp
    mov     rbp, rsp

    push    r15
    push    r14
    push    r13

    DEBUG   "Building page frame free-list"

    ; R15 = Pointer to memory map entry
    mov     r15, [memoryMap]
    ; R14 = Number of entries left to iterate over.
    mov     r14, [memoryMapLen]
    ; R13 = Pointer to `next` pointer of the last free-list entry/node to be
    ; updated. This is the "good taste" from Linus.
    lea     r13, [freeListHead]

    ; Construct the free-list by iterating over each entry of the memory map and
    ; creating a new node for each entry marked as available.
.loopCond:
    test    r14, r14
    jz      .loopOut
.loopTop:
    ; Skip any non-available RAM.
    cmp     QWORD [r15 + 0x10], 1
    jne     .loopNextIte

    ; Skip any entry that is fully contained before NEXT_FRAME_MIN_OFFSET, which
    ; is any entry for which base+length <= NEXT_FRAME_MIN_OFFSET.
    mov     rax, [r15]
    add     rax, [r15 + 0x8]
    cmp     rax, NEXT_FRAME_MIN_OFFSET
    jbe     .loopNextIte

    ; This entry is a good candidate for a node in the free-list. However,
    ; because the memory map is using byte granularity, the base might not be on
    ; a page/frame boundary and the length might not be a multiple of FRAME_SIZE
    ; bytes.
    ; Therefore we first need to compute the biggest memory range within this
    ; entry that is starting on a page/frame boundary and of length that is a
    ; multiple of FRAME_SIZE bytes.

    ; R11 = Current base of entry.
    mov     r11, [r15]
    ; R10 = Current size of entry.
    mov     r10, [r15 + 0x8]
    DEBUG   "Orig entry: base = $, size = $", r11, r10

    ; RCX = prev base. This will be used below.
    mov     rcx, r11

    ; Compute the new base, which is the base rounded up to the next page
    ; boundary, above NEXT_FRAME_MIN_OFFSET.
    ; R11 = roundup(max(base, NEXT_FRAME_MIN_OFFSET), FRAME_SIZE)
    mov     rax, NEXT_FRAME_MIN_OFFSET
    cmp     r11, rax
    cmovb   r11, rax
    test    r11, (FRAME_SIZE - 1)
    setnz   dl
    movzx   rdx, cl
    shl     rdx, 12
    add     r11, rdx

    ; RCX = Delta = new base - old base.
    sub     rcx, r11
    neg     rcx

    ; Now that we advanced the base forward (either to NEXT_FRAME_MIN_OFFSET, or
    ; the next frame boundary or both), check if we went over the original limit
    ; of the entry.
    cmp     r10, r11
    ; If length < delta then we went over the limit and this entry will not
    ; work, continue to the next iteration.
    jbe     .loopNextIte

    ; Compute the new size of the entry after changing the base.
    ; R10 = new size.
    sub     r10, rcx

    ; Make sure size is a multiple of FRAME_SIZE.
    and     r10, ~(FRAME_SIZE - 1)

    ; If the size is 0 at this point then this means the new size after updating
    ; base was less than FRAME_SIZE and therefore will not work, continue to
    ; next iteration.
    test    r10, r10
    jz      .loopNextIte

    ; Sanity check:
    ;   - base is on a page boundary
    ;   - size is multiple of FRAME_SIZE
    test    r11, (FRAME_SIZE - 1)
    jnz     .sanityCheckFailed
    test    r10, (FRAME_SIZE - 1)
    jz      .sanityCheckOk
.sanityCheckFailed:
    PANIC   "Computed base not on page boundary"
.sanityCheckOk:

    ; We now have a memory area that is available to use, with:
    ;   - Base = r11 which is on a page-boundary.
    ;   - Size = r10 which is a multiple of FRAME_SIZE.
    ; Compute the size in units of frames.
    shr     r10, 12

    ; Allocate a new node in the list.
    ; RAX = Pointer to new node.
    push    r10
    push    r11

    mov     rdi, FL_NODE_SIZE
    call    malloc

    ; Set base to r11
    pop     QWORD [rax + FL_BASE]
    ; Set num pages to r10
    pop     QWORD [rax + FL_NUM_PAGES]
    ; Initialize the `next` pointer to NULL, just in case this is the last entry
    ; in the list.
    xor     rdx, rdx
    mov     [rax + FL_NEXT], rdx

    DEBUG   "Free-list entry @$: base = $, pages = $", \
        rax, QWORD [rax + FL_BASE], QWORD [rax + FL_NUM_PAGES]

    ; Update the previous node's `next` pointer to point to this new node.
    mov     [r13], rax

    ; R13 = Pointer to the `next` pointer to update on next iteration.
    lea     r13, [rax + FL_NEXT]
.loopNextIte:
    dec     r14
    add     r15, 0x18
    jmp     .loopCond
.loopOut:

    ; Log the free-list.
    INFO    "Physical page frame free-list:"
    ; RAX = Pointer to current node.
    mov     rax, [freeListHead]
.printLoopCond:
    test    rax, rax
    jz      .printLoopOut

    INFO    "  .base = $ num_pages = $", QWORD [rax + FL_BASE], \
        QWORD [rax + FL_NUM_PAGES]

    mov     rax, [rax + FL_NEXT]
    jmp     .printLoopCond
.printLoopOut:

    pop     r13
    pop     r14
    pop     r15

    leave
    ret

; ==============================================================================
; Allocate a frame in available memory. This function uses the memory map to
; find available frames and as such cannot be called prior to calling
; parseMemoryMap.
; @return (RAX): The physical address of the allocated frame.
DEF_GLOBAL_FUNC64(allocFrame):
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

    ; Initialize the allocator and free-list upon the first call to allocFrame.
    mov     rax, [freeListHead]
    test    rax, rax
    jnz     .freeListInitialized
    call    initFreeList
.freeListInitialized:

    ; Find the first entry with NUM_PAGES > 0.

    ; R11 = Pointer to free-list entry/node.
    mov     r11, [freeListHead]

.loopCond:
    test    r11, r11
    jz      .loopOutFailed
.loopTop:
    ; Check if current entry contains at least one page.
    ; RCX = Number of pages in entry
    mov     rcx, [r11 + FL_NUM_PAGES]
    test    rcx, rcx
    jz      .loopNextIte

    ; The entry contains at least one page. Allocate the next page in the memory
    ; region.
    ; RAX = Address of newly allocated frame.
    mov     rax, [r11 + FL_BASE]

    ; Update the base and size of the entry.
    add     QWORD [r11 + FL_BASE], FRAME_SIZE
    dec     QWORD [r11 + FL_NUM_PAGES]

    ; Allocation was successful, return
    jmp     .loopOut
.loopNextIte:
    ; Advance to next free list entry.
    mov     r11, [r11 + FL_NEXT]
    jmp     .loopCond
.loopOutFailed:
    PANIC   "Failed to allocate frame, no free frame available."
.loopOut:
    leave
    ret
