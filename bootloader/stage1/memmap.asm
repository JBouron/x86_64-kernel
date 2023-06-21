; Routines related to memory map parsing.

%include "malloc.inc"
%include "bioscall.inc"

SECTION .data
; Number of entries in the memory map.
memoryMapLen:
DQ  0x0
; The memory map, an array of `memoryMapLen` entries with the following layout:
;   {
;       u64 base;
;       u64 length; // in bytes.
;       u64 type;
;   }
memoryMap:
DQ  0x0

SECTION .text

BITS    64

; Size in bytes of an Adress Range Descriptor Structure returned by the BIOS
; function parsing the memory map.
; The size of this structure needs to be at least 20 bytes. Some BIOS supports
; sizes up to 24 bytes. It is recommended to use 24 bytes, in the worst case the
; BIOS only fills the first 20 bytes.
ARDS_SIZE   EQU 24

; ==============================================================================
; Handle an entry parsed from the memory map. Incrementally creates an array of
; Adress Range Descriptor Structure in memory. The address of this array is
; stored under [memoryMap], its length (number of entries) under [memoryMapLen].
; @param %RDI: Pointer to Adress Range Descriptor Structure last read from
; memory map.
_handleMemoryMapEntry:
    push    rbp
    mov     rbp, rsp

    push    rdi
    ; Add the entry to the saved map.
    ; Allocate a new entry in the array.
    mov     rdi, ARDS_SIZE
    call    malloc
    pop     rdi

    ; Check if memoryMap has already been initialized.
    test    QWORD [memoryMap], -1
    jnz     .addToArray
    ; This is the first ARDS we are processing. Update memoryMap to point to the
    ; array.
    mov     [memoryMap], rax

.addToArray:
    ; Sanity check: The malloc call must have returned the address right after
    ; the last entry added to memoryMap. We rely in this assumption to build a
    ; contiguous array of entries.
    mov     r8, [memoryMapLen]
    imul    r8, ARDS_SIZE
    add     r8, [memoryMap]
    cmp     r8, rax
    je      .assertOk
    ; The allocated entry does not immediately follow the last entry.
    PANIC   "_handleMemoryMapEntry: malloc does not follow last entry."
.assertOk:
    ; Copy the the ARDS to the allocated elem in the array.
    mov     rsi, rdi
    mov     rdi, rax
    mov     rcx, ARDS_SIZE
    cld
    rep movsb

    ; Update the number of entry in the array.
    inc     QWORD [memoryMapLen]

    leave
    ret

; ==============================================================================
; Parse the memory map as reported by the BIOS function INT=15h AX=E820h.
DEF_GLOBAL_FUNC(parseMemoryMap):
    push    rbp
    mov     rbp, rsp

    ; Alloc an Adress Range Descriptor Structure (ARDS) on the stack. This
    ; struct will be used for every call to INT 15h E820h, containing the
    ; response range.
    sub     rsp, ARDS_SIZE

    ; Alloc a BIOS call packet on the stack to be used for every INT 15h E820h
    ; calls. This packet will be used by all call to the BIOS function.
    sub     rsp, BCP_SIZE
    mov     BYTE [rsp + BCP_IN_INT_OFF], 0x15
    mov     DWORD [rsp + BCP_INOUT_EBX_OFF], 0x0
    mov     DWORD [rsp + BCP_INOUT_ECX_OFF], ARDS_SIZE
    mov     DWORD [rsp + BCP_INOUT_EDX_OFF], 0x534D4150
    ; Address of the allocated ARDS to fill in.
    mov     rax, rsp
    add     rax, BCP_SIZE
    mov     [rsp + BCP_INOUT_EDI_OFF], eax

    ; Call the memory map function as long as we haven't reached the last entry
    ; or got an error back.
.loopEntries:
    ; Output values of the previous call to the memory map function will clobber
    ; the register value for EAX in the BCP, need to reset it at every call.
    mov     DWORD [esp + BCP_INOUT_EAX_OFF], 0xe820

    ; Call INT 15h E820h.
    mov     rdi, rsp
    call    callBiosFunc

    ; Check for errors.
    mov     al, [rsp + BCP_OUT_JC]
    test    al, al
    jz      .callOk
    CRIT    "INT 15h,AXX=E820h: Failed to call BIOS function"
    jmp     .done
.callOk:

    ; Check that the signature in the output value of EAX is correct.
    mov     eax, [rsp + BCP_INOUT_EAX_OFF]
    cmp     eax, 0x534D4150
    je      .sigOk
    CRIT    "INT 15h,AXX=E820h: Invalid signature in output EAX value"
    jmp     .done

.sigOk:
    mov     rdi, rsp
    add     rdi, BCP_SIZE
    call    _handleMemoryMapEntry

    ; The output EBX contains the value of the next signature for the next call
    ; to the BIOS function, hence we don't need to write it ourselves to the BCP
    ; struct.
    ; An output continuation of 0x0 means this was the last entry in the map.
    mov     eax, [esp + BCP_INOUT_EBX_OFF]
    test    eax, eax
    jz      .done

    jmp     .loopEntries
.done:

    ; Sanitize the memory map.
    mov     rdi, [memoryMap]
    mov     rsi, [memoryMapLen]
    call    sanitizeMemMap
    ; Replace the map with the sanitized one.
    mov     [memoryMap], rax
    mov     [memoryMapLen], rdx

    ; Print the memory map.
    mov     rcx, rdx
.printLoop:
    mov     rdx, [rax]
    add     rdx, [rax + 0x8]
    dec     rdx
    INFO    "  $ - $ Type: $", QWORD [rax], rdx, QWORD [rax + 0x10]
    add     rax, ARDS_SIZE
    loop    .printLoop

    leave
    ret

; We use QWORD bitmaps within sanitizeMemMap in order to compute overlaps, hence
; we are limited by the number of bits in a QWORD. This should be fine however,
; as typical memmaps have around 10 entries.
; In case sanitizeMemMap PANIC's due to bumping this limit, the code would need
; to be rewritten to handle bitmaps bigger than 64-bits.
MAX_MEMMAP_ENTRIES  EQU 64

; ==============================================================================
; Sanitize a memory map by resolving all overlaps and sorting the entries.
;   * Overlap resolution: E820 is not _required_ to return non-overlapping
;   entries in its memory map. This means it is possible to have two entries of
;   different types which are overlapping. In this situation the actual type of
;   the bytes in the overlapping region are of the most restrictive type of the
;   two entries. For example, the following map with two entries:
;       map[0] = {base = 1000, len = 1000, type = 1} and
;       map[1] = {base = 1500, len = 1000, type = 2}
;   contains two overlapping entries. The equivalent map with no overlapping
;   entries would be:
;       map[0] = {base = 1000, len = 500,  type = 1}
;       map[1] = {base = 1500, len = 1000, type = 2}
;   Of course in a real memory map there could be more than two overlapping
;   regions.
;   * Sorting entries: Once all overlaps are resolved we can sort the entries of
;   the map on their `base` address. This makes working with the map a bit
;   easier when allocating physical frames for instance.
; @param %rdi: Pointer to memory map, e.g. an array of ARDS.
; @param %rsi: Number of elements in the memory map. This number must be less or
; equal to MAX_MEMMAP_ENTRIES.
; @return %rax: Pointer to a _new_ array containing the sanitized memory map.
; Note that the array passed as argument is unchanged.
; @return %rdx: Number of elements in the new array.
DEF_GLOBAL_FUNC(sanitizeMemMap):
    ; The algorithm
    ; -------------
    ;
    ; Assume the following memory map where each entry is represented as a
    ; range, and the x-axis is physical offset:
    ;          +--------------------------------------------+
    ;   map[0]:|    111111111111111                         |
    ;   map[1]:|              22222222                      |
    ;   map[2]:|       1111111111111111111111               |
    ;          +--------------------------------------------+
    ;          ^                                            ^
    ;         0x0                                           0xffffffffffffffff
    ; Each map entry is represented as a sequence of 111 or 222 depending its
    ; type. The sequence spans the bytes covered by the entry, e.g. bytes in the
    ; range [base; base+len[.
    ;
    ; The goal of the algorithm is to transform this range into the following:
    ;          +--------------------------------------------+
    ;   map[0]:|    1111111111                              |
    ;   map[1]:|              22222222                      |
    ;   map[2]:|                      111111                |
    ;          +--------------------------------------------+
    ;          ^                                            ^
    ;         0x0                                           0xffffffffffffffff
    ;
    ; A naive way to build this map would be to have a "cursor" starting at
    ; offset 0x0. A each iteration we check in which entry/ies this cursor falls
    ; into and then compute the type of the byte under the cursor as the max of
    ; all the types for those entries, e.g. the most restrictive type. Then we
    ; increment the cursor by one (e.g. move to the next byte) and we repeat.
    ; This is of course very inefficient, but we can make it much better with a
    ; simple change: instead of incrementing the position of the cursor by one
    ; we can jump to the next "boundary", e.g. the offset where an entry of the
    ; map is starting or ending.

    ; Before introducing the algorithm, we introduce the following helper
    ; function: findNextBoundary(offset). This function takes an offset as
    ; argument and returns a tuple (boundaryOffset, bitmap). boundaryOffset is
    ; the offset of the next boundary that is >= offset, a boundary is either
    ; the base offset of an entry in the map or its end offset computed as base
    ; + length. bitmap is a bitmap indicating to which entry/entries this
    ; boundary belongs to, in other words if bit i is set in the bitmap then
    ; either map[i].base == boundaryOffset OR map[i].base + map[i].length ==
    ; boundaryOffset.
    ;
    ; The full algorithm to build the new map looks as follows:
    ;   1. LET C the current offset of the cursor and B the bitmap of the
    ;   entries that contains the cursor.
    ;   2. SET C, B = findNextBoundary(0x0)
    ;   3. WHILE for all i, C < map[i].base + map[i].length:
    ;       3.1. E, Be = findNextBoundary(C+1)
    ;       3.2. IF B != 0
    ;           3.2.1. LET T = max(map[i].type) for all i set in B.
    ;           3.3.2. OUTPUT new entry {base = C, length = E - C, type = R} in
    ;           new map.
    ;       3.3. C = E
    ;       3.4. B = B ^ Be
    ;
    ; Notes:
    ;   * To make things simpler, we limit the size of our bitmaps to 64-bits so
    ;   that it fits in a single register. This means that memory maps can only
    ;   contain 64 entries maximum. Typical memory maps contains < 10 entries so
    ;   this should be fine.
    ;   * When outputing a new entry in the new memmap we first check if it can
    ;   be merged with the last entry of the memmap. Merging two entries is only
    ;   possible if they are of the same type and contiguous.
    push    rbp
    mov     rbp, rsp
    xor     rax, rax
    ; QWORD [rbp - 0x8] = Address of new memory map.
    push    rax
    ; QWORD [rbp - 0x10] = Size of new memory map.
    push    rax

    ; Callee-saved regs.
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx

    cmp     rsi, MAX_MEMMAP_ENTRIES
    jbe     .memmapSizeOk
    PANIC   "Memory map is too big to be sanitized needs to be less than 65 ent"
.memmapSizeOk:

    ; R15 = C
    xor     r15, r15
    ; R14 = B
    xor     r14, r14
    ; R13 = map
    mov     r13, rdi
    ; R12 = map size
    mov     r12, rsi

    ; Compute the termination condition for the algorithm's while loop.
    ; RBX = Condition termination = max(map[i].base + map[i].length)
    xor     rbx, rbx
    mov     rax, rdi
    mov     rcx, rsi
.computeTerminationLoop:
    mov     rdx, [rax]
    add     rdx, [rax + 0x8]
    cmp     rdx, rbx
    cmova   rbx, rdx
.computeTerminationLoopNextIte:
    add     rax, ARDS_SIZE
    loop    .computeTerminationLoop

    ; Now for the main algo
    ; Set initial value of C and B by calling findNextBoundary for the first
    ; time.
    mov     rdi, r13
    mov     rsi, r12
    xor     rdx, rdx
    call    findNextBoundary
    mov     r14, rdx
    mov     r15, rax

.loopCond:
    cmp     r15, rbx
    jae     .loopOut
.loopTop:
    ; Compute E and Be.
    mov     rdi, r13
    mov     rsi, r12
    lea     rdx, [r15 + 1]
    call    findNextBoundary
    ; R9 = E
    mov     r9, rax
    ; R8 = Be
    mov     r8, rdx

    ; Sanity check: We should not see an empty bitmap here since we never try to
    ; find a next boundary when none exists due to the condition of the loop.
    test    r8, r8
    jnz     .ok
    PANIC   "Expected at least one more boundary in memmap"
.ok:

    ; If B is non empty then we are under a range/entry of the bitmap and
    ; therefore we know the type of that memory. Otherwise we are in a hole and
    ; we cannot assume what type it is, just skip it in this case.
    test    r14, r14
    jz      .loopNextIte

    ; We are not in a hole, compute the type of memory for that range which is
    ; the max of the type of all entries that have their bit set in B.
    ; Save caller-saved regs.
    push    r8
    push    r9
    mov     rdi, r13
    mov     rsi, r12
    mov     rdx, r14
    ; RAX = most restrictive type / new entry type.
    call    typeFromBitmap
    pop     r9
    pop     r8

    ; Check if there is already at least one in the new memmap.
    test    QWORD [rbp - 0x10], -1
    jz      .addNewEntry

    ; This is not the first entry we are trying to insert. In this situation we
    ; might be able to merge this new entry with the last entry in the memmap.
    ; We can only do this if:
    ;   * Both entries have the same type.
    ;   * This new entry's base is the end offset of the last entry, that is the
    ;   entries are contiguous.
    ; RCX = pointer to last entry.
    mov     rcx, [rbp - 0x10]
    dec     rcx
    imul    rcx, ARDS_SIZE
    add     rcx, [rbp - 0x8]
    cmp     [rcx + 0x10], rax
    jne     .addNewEntry
    ; The type of the new entry is the same as the last. Check that they are
    ; contiguous.
    ; RDX = end offset of last entry.
    mov     rdx, [rcx]
    add     rdx, [rcx + 0x8]
    cmp     rdx, r15
    jne     .addNewEntry
    ; The two entries are contiguous, we can do the merge. Merging simply
    ; involves updating the length field of the last entry in the memmap.
    mov     rax, r9
    sub     rax, [rcx]
    mov     [rcx + 0x8], rax

    jmp     .loopNextIte

.addNewEntry:
    ; The type of the new entry does not match the type of the last one. We need
    ; to allocate a new entry in the memmap in this case, leaving the last one
    ; untouched.
    push    rax
    push    r8
    push    r9
    mov     rdi, ARDS_SIZE
    call    malloc
    pop     r9
    pop     r8
    ; RDX = type of new entry (e.g. old RAX)
    pop     rdx

    ; If this is the first entry, save it to the local var holding the pointer
    ; to the new array.
    test    QWORD [rbp - 0x8], -1
    jnz     .addNewEntryCont
    ; First entry.
    mov     [rbp - 0x8], rax
    jmp     .addNewEntryCont
.addNewEntryCont:
    ; Construct the new entry.
    ; Base
    mov     [rax], r15
    ; Len = End - base
    mov     [rax + 0x8], r9
    sub     [rax + 0x8], r15
    ; Type
    mov     [rax + 0x10], rdx

    ; Update number of entries.
    inc     QWORD [rbp - 0x10]

    ; Fall-through
.loopNextIte:
    mov     r15, r9
    xor     r14, r8
    jmp     .loopCond
.loopOut:

    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    mov     rax, [rbp - 0x8]
    mov     rdx, [rbp - 0x10]

    DEBUG   "Sanitized memmap @$ with $ entries", rax, rdx

    leave
    ret

; ==============================================================================
; Find the next boundary in the memory map that is bigger than the given offset.
; See comment in sanitizeMemMap to understand how/why this function is used.
; @param %rdi: Pointer to the memory map, array of ARDS.
; @param %rsi: Number of entries in the memory map.
; @param %rdx: Start offset to search from. The returned offset is >= this
; offset.
; @return %rax: The offset of the next boundary.
; @return %rdx: The bitmap in which bit i is set if the map[i] starts or ends on
; the offset in %rax. If no boundary was found this is 0.
DEF_GLOBAL_FUNC(findNextBoundary):
    push    rbp
    mov     rbp, rsp
    ; R11 = known min boundary offset.
    mov     r11, -1
    ; R10 = bitmap to return.
    mov     r10, 0
    ; RDI = Pointer to entries in the map (already set).
    ; RSI = Number of entries left to examine (already set).
    ; RDX = Requested min offset (already set).
    ; R9 = bit mask for current entry.
    mov     r9, 1
.loopCond:
    test    rsi, rsi
    jz      .loopOut
.loopTop:
    ; RAX = base of current entry.
    mov     rax, [rdi]
    ; Check if >= the requested offset.
    cmp     rax, rdx
    jb      .checkEndOffset
    ; We have offset <= base. Check against the min. Note that in this case we
    ; don't need to check the end boundary of this entry because it will
    ; obviously be bigger than the start boundary.
    jmp     .updateMinBitmapAndGoToNextIte

.checkEndOffset
    ; No luck with the base, note that we can only get here if the base was
    ; smaller than the requested minimum offset (RDX).
    ; RAX = end boundary. RAX already contains the base here so we only need to
    ; add the length.
    add     rax, [rdi + 0x8]
    ; Check if >= requested offset.
    cmp     rax, rdx
    jb      .loopNextIte
    ; The end boundary is bigger than the requested offset, check against the
    ; known min boundary.
    jmp     .updateMinBitmapAndGoToNextIte

    ; This is a helper code block to avoid code duplication. This block expects
    ; RAX to contain the current candidate for the min boundary and updates the
    ; curr known min and the bitmap accordingly before jumping to the next
    ; iteration.
.updateMinBitmapAndGoToNextIte:
    ; Compare again known currMin.
    cmp     rax, r11
    ja      .loopNextIte
    ; We have boundary <= currMin. If the boundary is < to the currMin then this
    ; means we found an earlier boundary than before and we need to clear the
    ; bitmap. If the boundary is == to the currMin then we set the bit in the
    ; bitmap for this entry without clearing the bitmap first; this is needed in
    ; case two entries are sharing boundaries in which case we want the bitmap
    ; to contain both bits.
    jb      .foundEarlierBoundary
    ; Case: boundary == currMin. Set bit in bitmap but don't reset it.
    or     r10, r9
    jmp     .loopNextIte
.foundEarlierBoundary:
    ; Case: boundary < currMin, reset bitmap and update currMin.
    mov     r10, r9
    mov     r11, rax
    ; Fall-through (jmp     .loopNextIte)

.loopNextIte:
    dec     rsi
    add     rdi, ARDS_SIZE
    shl     r9, 1
    jmp     .loopCond

.loopOut:
    mov     rax, r11
    mov     rdx, r10
    leave
    ret

; ==============================================================================
; Given a memory map and a bitmap, compute the most restrictive type, that is
; the max of the type of all entries which have their bit set in the bitmap.
; @param %RDI: Pointer to the memory map.
; @param %RSI: Number of entries in the map.
; @param %RDX: Bitmap value.
; @return %RAX: The most restrictive type.
DEF_GLOBAL_FUNC(typeFromBitmap):
    push    rbp
    mov     rbp, rsp
    ; R11 = loop iteration / mask of current entry.
    mov     r11, 1
    ; R10D = curr max known type.
    xor     r10d, r10d
    ; RDI = Pointer to current entry (already set).
    ; RSI = Loop counter (len of memmap down to 0)
    ; RDX = bitmap (already set)
.loopCond:
    test    rsi, rsi
    jz      .loopOut
    ; As a small optimization we can already check if there are still bits we
    ; haven't checked in the bitmap.
    ; If (currMask - 1) & bitmap == 0 then this means no more bits on the left
    ; of the current bit (including the current bit).
    mov     rax, r11
    dec     rax
    not     rax
    test    rax, rdx
    jz      .loopOut
.loopTop:
    ; Check if current bit set in bitmap.
    test    r11, rdx
    jz      .loopNextIte
    ; Current entry has its bit set, update the max known type.
    cmp     DWORD [rdi + 0x10], r10d
    cmova   r10d, DWORD [rdi + 0x10]
.loopNextIte:
    add     rdi, ARDS_SIZE
    shl     r11, 1
    jmp     .loopCond
.loopOut:
    mov     eax, r10d
    leave
    ret
