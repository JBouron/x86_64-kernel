; Routines related to memory map parsing.

%include "malloc.inc"
%include "bioscall.inc"

SECTION .data
; Number of entries in the memory map.
memoryMapLen:
DW  0x0
; The memory map, an array of `memoryMapLen` entries with the following layout:
;   {
;       u64 base;
;       u64 length; // in bytes.
;       u64 type;
;   }
memoryMap:
DW  0x0

SECTION .text

BITS    32

; ==============================================================================
; Parse the memory map as reported by the BIOS function INT=15h AX=E820h.
DEF_GLOBAL_FUNC(parseMemoryMap):
    push    ebp
    mov     ebp, esp
    push    ebx

    ; DWORD [EBP - 0x4] = Pointer to first Adress Range Descriptor Structure
    ; (ARDS) in the array of ARDS built when parsing the memory map.
    ; This pointer is initialized to NULL and updated upon the first ARDS is
    ; parsed.
    push    0x0

    ; DWORD [EBP - 0x8] = Number of ARDS in the array.
    push    0x0

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
    LOG     "INT 15h,AXX=E280h: Failed to call BIOS function"
    jmp     .done
.callOk:

    ; Check that the signature in the output value of EAX is correct.
    mov     eax, [esp + BCP_INOUT_EAX_OFF]
    cmp     eax, 0x534D4150
    je      .sigOk
    LOG     "INT 15h,AXX=E280h: Invalid signature in output EAX value"
    jmp     .done

.sigOk:
    LOG     "  Base = $_$ Len = $_$ Type = $", \
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
    LOG     "Mem map @$ len = $ entries", DWORD [ebp - 0x4], DWORD [ebp - 0x8]

    ; Save the memory map for later use.
    mov     eax, [ebp - 0x4]
    mov     [memoryMap], eax
    mov     eax, [ebp - 0x8]
    mov     [memoryMapLen], eax

    pop     ebx
    leave
    ret
