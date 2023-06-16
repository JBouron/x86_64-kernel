; Routines related to memory map parsing.

%include "malloc.inc"
%include "bioscall.inc"
%include "arith64.inc"

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

    ; RAX = Base
    mov     rax, [rdi]
    ; RCX = End, e.g. offset of the last byte in the range = Base + len - 1
    mov     rcx, [rdi + 0x8]
    add     rcx, rax
    dec     rcx
    ; RDX = type
    mov     rdx, [rdi + 0x10]
    INFO    "  $ - $ Type: $", rax, rcx, rdx

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
    CRIT    "_handleMemoryMapEntry: malloc does not follow last entry."
.dead:
    ; Guess what time it is? That's right! It's time to write a PANIC macro.
    hlt
    jmp     .dead
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
    CRIT    "INT 15h,AXX=E280h: Failed to call BIOS function"
    jmp     .done
.callOk:

    ; Check that the signature in the output value of EAX is correct.
    mov     eax, [rsp + BCP_INOUT_EAX_OFF]
    cmp     eax, 0x534D4150
    je      .sigOk
    CRIT    "INT 15h,AXX=E280h: Invalid signature in output EAX value"
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

    ; FIXME: Here we are making two assumptions:
    ;   i) The memory map, as reported by the BIOS is sorted on the base address
    ;   of the ARDS.
    ;   ii) No two elements of the memory map are overlapping.
    ; In practice those assumptions seem to hold. However per the BIOS
    ; function's specification, the BIOS is not guaranteeing those.
    ; At some point we should therefore sort and merge adjacent ranges. For now
    ; we simply make a sanity check that those assumptions are holding.
    mov     rdi, [memoryMap]
    mov     rsi, [memoryMapLen]
    call    checkMemoryMapInvariants
    test    rax, rax
    jnz     .invariantsOk
    ; Assumptions did not hold.
    CRIT    "Memory map is either not sorted or has overlapping ranges"
.dead:
    hlt
    jmp     .dead
.invariantsOk:
    DEBUG   "Memory map invariant ok, mem map size = $ entries", \
        QWORD [memoryMapLen]
    leave
    ret

BITS    64
; ==============================================================================
; Check that the memory map given as argument follows both invariants:
;   i ) All entries are sorted on their base address.
;   ii) No entries are overlapping.
; @param %RDI: Array of entries.
; @param %RSI: Number of entries.
checkMemoryMapInvariants:
    push    rbp
    mov     rbp, rsp

    cmp     rsi, 2
    jae     .multipleEntries
    ; There is only a single entry, this is the trivial case and the map is
    ; obviously correct.
    mov     rax, 1
    jmp     .out
.multipleEntries:
    ; There are multiple entries, for each entry i we need to check that:
    ;   - map[i].base < map[i+1].base, e.g. the entries are sorted.
    ;   - map[i].base + map[i].len <= map[i+1].base, e.g. the entries are not
    ;   overlapping.

    ; RCX = loop counter / i
    xor     rcx, rcx
    ; RDI = Pointer to current entry (already pointing to it).
.loopCond:
    mov     rax, rsi
    dec     rax
    cmp     rcx, rax
    je      .loopOut
.loopStart:
    ; Check that map[i].base < map[i+1].base
    ; RAX = map[i].base
    mov     rax, [rdi]
    ; RDX = map[i+1].base
    mov     rdx, [rdi + ARDS_SIZE]

    cmp     rax, rdx
    jb      .expectedRes
    WARN    "map[i].base >= map[i+1].base for i = $", rcx
    xor     rax, rax
    jmp     .out
.expectedRes:
    ; We have map[i].base < map[i+1].base. Now check for:
    ;   map[i].base + map[i].len <= map[i+1].base
    ; RAX is still map[i].base at this point.
    add     rax, [rdi + 8]
    ; RDX is still map[i+1].base at this point. We can now compare.
    cmp     rax, rdx
    jbe     .expectedRes2
    WARN    "map[i+1].base < map[i].base + map[i].len for i = $", rcx
    xor     rax, rax
    jmp     .out
.expectedRes2:
    ; The ranges are not overlapping. Check the next pair of range.
    add     rdi, ARDS_SIZE
    inc     rcx
    jmp     .loopCond
.loopOut:
    ; All ranges are good, success return true.
    mov     rax, 1
.out:
    leave
    ret
