; Routines related to memory map parsing.

%include "malloc.inc"
%include "bioscall.inc"
%include "arith64.inc"

SECTION .data
; Number of entries in the memory map.
memoryMapLen:
DD  0x0
; The memory map, an array of `memoryMapLen` entries with the following layout:
;   {
;       u64 base;
;       u64 length; // in bytes.
;       u64 type;
;   }
memoryMap:
DD  0x0

SECTION .text

BITS    32

; ==============================================================================
; Parse the memory map as reported by the BIOS function INT=15h AX=E820h.
DEF_GLOBAL_FUNC(parseMemoryMap):
    push    ebp
    mov     ebp, esp

    ; DWORD [EBP - 0x4] = Pointer to first Adress Range Descriptor Structure
    ; (ARDS) in the array of ARDS built when parsing the memory map.
    ; This pointer is initialized to NULL and updated upon the first ARDS is
    ; parsed.
    push    0x0

    ; DWORD [EBP - 0x8] = Number of ARDS in the array.
    push    0x0

    ; Callee-saved.
    push    ebx

    ; Alloc a BIOS call packet on the stack to be used for every INT 15h E820h
    ; calls. This packet will be used by all call to the BIOS function.
    sub     esp, BCP_SIZE
    mov     BYTE [esp + BCP_IN_INT_OFF], 0x15
    mov     DWORD [esp + BCP_INOUT_EBX_OFF], 0x0
    mov     DWORD [esp + BCP_INOUT_ECX_OFF], 24
    mov     DWORD [esp + BCP_INOUT_EDX_OFF], 0x534D4150

    ; Call the memory map function as long as we haven't reached the last entry
    ; or got an error back.
.loopEntries:
    ; Output values of the previous call to the memory map function will clobber
    ; the register values in the BCP, need to reset them.
    mov     DWORD [esp + BCP_INOUT_EAX_OFF], 0xe820
    ; Also, for each call use a freshly allocated Adress Range Descriptor
    ; Structure (ARDS). Because malloc always allocates contiguously, we end up
    ; with an array of ARDS.
    ; EBX = Address of ARDS.
    push    24
    call    malloc
    add     esp, 0x4
    mov     ebx, eax
    ; Put the address of the allocated ARDS in the BIOS call inputs.
    mov     [esp + BCP_INOUT_EDI_OFF], ebx
    mov     ecx, [ebp - 0x4]
    test    ecx, ecx
    jnz     .doCall
    ; This is the first allocated ARDS, save the start offset of the array.
    mov     [ebp - 0x4], ebx
.doCall:

    push    esp
    call    callBiosFunc
    add     esp, 4

    ; Check for errors.
    mov     al, [esp + BCP_OUT_JC]
    test    al, al
    jz      .callOk
    CRIT    "INT 15h,AXX=E280h: Failed to call BIOS function"
    jmp     .done
.callOk:

    ; Check that the signature in the output value of EAX is correct.
    mov     eax, [esp + BCP_INOUT_EAX_OFF]
    cmp     eax, 0x534D4150
    je      .sigOk
    CRIT    "INT 15h,AXX=E280h: Invalid signature in output EAX value"
    jmp     .done

.sigOk:
    INFO    "  Base = $_$ Len = $_$ Type = $", \
        DWORD [ebx + 0x4], DWORD [ebx + 0x0], \
        DWORD [ebx + 0xc], DWORD [ebx + 0x8], \
        DWORD [ebx + 0x10]

    ; Update the count of ARDS in the array.
    inc     DWORD [ebp - 0x8]

    ; The output EBX contains the value of the next signature for the next call
    ; to the BIOS function, hence we don't need to write it ourselves to the BCP
    ; struct.
    ; An output continuation of 0x0 means this was the last entry in the map.
    mov     eax, [esp + BCP_INOUT_EBX_OFF]
    test    eax, eax
    jz      .done

    jmp     .loopEntries
.done:
    INFO    "Mem map @$ len = $ entries", DWORD [ebp - 0x4], DWORD [ebp - 0x8]

    ; Save the memory map for later use.
    mov     eax, [ebp - 0x4]
    mov     [memoryMap], eax
    mov     eax, [ebp - 0x8]
    mov     [memoryMapLen], eax

    ; FIXME: Here we are making two assumptions:
    ;   i) The memory map, as reported by the BIOS is sorted on the base address
    ;   of the ARDS.
    ;   ii) No two elements of the memory map are overlapping.
    ; In practice those assumptions seem to hold. However per the BIOS
    ; function's specification, the BIOS is not guaranteeing those.
    ; At some point we should therefore sort and merge adjacent ranges. For now
    ; we simply make a sanity check that those assumptions are holding.
    push    DWORD [memoryMapLen]
    push    DWORD [memoryMap]
    call    checkMemoryMapInvariants
    add     esp, 8
    test    eax, eax
    jnz     .invariantsOk
    ; Assumptions did not hold.
    CRIT    "Memory map is either not sorted or has overlapping ranges"
.dead:
    hlt
    jmp     .dead
.invariantsOk:

    pop     ebx
    leave
    ret

; ==============================================================================
; Check that the memory map given as argument follows both invariants:
;   i ) All entries are sorted on their base address.
;   ii) No entries are overlapping.
; @param (DWORD): Array of entries.
; @param (DWORD): Number of entries.
checkMemoryMapInvariants:
    push    ebp
    mov     ebp, esp

    push    ebx
    push    esi

    ; Memory maps of length 1 are naturally following the invariants.
    cmp     DWORD [ebp + 0xc], 1
    je      .out

    ; EBX = Pointer to entries.
    mov     ebx, [ebp + 0x8]
    ; ESI = Iteration counter.
    xor     esi, esi

    ; For i = 0; i < len(map)-1; ++i;
.loopCond:
    mov     eax, [ebp + 0xc]
    dec     eax
    cmp     esi, eax
    je      .loopOut
.loopStart:
    ; Check that map[i].base < map[i+1].base
    ; EAX = &map[i].base
    mov     eax, esi
    imul    eax, 24
    add     eax, ebx
    ; Second operand for cmp64
    push    eax
    ; EAX = &map[i+1].base. Computing this is easier, just add the size of an
    ; entry since EAX already points to the previous entry's base address.
    add     eax, 24
    ; First operand for cmp64
    push    eax

    call    cmp64
    add     esp, 8
    cmp     eax, 1
    je      .expectedRes
    ; The base address of map[i+1] is not > map[i].base. This is a violation of
    ; our assumption. Return false.
    CRIT    "map[i].base >= map[i+1].base for i = $", esi
    xor     eax, eax
    jmp     .out
.expectedRes:
    ; We have map[i].base < map[i+1].base. Now check that map[i] and map[i+1]
    ; do not overlap; we do this by making sure that:
    ;   map[i].base + map[i].len <= map[i+1].base
    ; First compute map[i].base + map[i].len.
    ; Make some space for the result of the add.
    sub     esp, 8
    ; EAX = &map[i].base
    mov     eax, esi
    imul    eax, 24
    add     eax, ebx
    ; Push second add operand.
    push    eax
    ; EAX = &map[i].len
    add     eax, 8
    ; Push first operand for add64.
    push    eax
    ; Push destination for add64, allocated above.
    lea     eax, [esp + 8]
    push    eax
    ; Add the 64-bit values.
    call    add64
    ; Clear args (3).
    add     esp, 12
    ; Now ESP points to the 64-bit value containing the result of map[i].base +
    ; map[i].len. Compare it against map[i+1].base.
    ; Push cmp64 second operand, e.g. map[i].base + map[i].len
    push    esp
    ; EAX = &map[i+i].base
    mov     eax, esi
    inc     eax
    imul    eax, 24
    add     eax, ebx
    ; Push first cmp64 operand
    push    eax

    call    cmp64
    ; Clear args (2).
    add     esp, 8

    ; We want either:
    ;   * map[i+1].base > map[i].base + map[i].len
    ;   * map[i+1].base == map[i].base + map[i].len
    ; In other words the result of the cmp64 should either be 1 or 0 (given that
    ; map[i+1].base was the first operand), so NOT -1.
    cmp     eax, -1
    jne     .expectedRes2
    ; We have map[i+1].base < map[i].base + map[i].len. Invariant violated.
    CRIT    "map[i+1].base < map[i].base + map[i].len for i = $", esi
    xor     eax, eax
    jmp     .out
.expectedRes2:
    ; The ranges are not overlapping. Check the next pair of range.
    ; Clear the space allocated on the stack for the add64.
    add     esp, 8
    ; fall-through.
.loopNext:
    inc     esi
    jmp     .loopCond
.loopOut:
    ; All ranges are good, success return true.
    mov     eax, 1
.out:
    pop     esi
    pop     ebx

    leave
    ret
