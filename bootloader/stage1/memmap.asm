; Routines related to memory map parsing.

%include "malloc.inc"
%include "bioscall.inc"

BITS    32

; 
DEF_GLOBAL_FUNC(printMemoryMap):
    push    ebp
    mov     ebp, esp

    ; Allocate the Address Range Descriptor Structure and the BCP structure on
    ; the stack. All calls use the same structures.
    sub     esp, 24
    ; EBX = Address Range Descriptor Structure address.
    mov     ebx, esp
    ; Alloc a BIOS call packet on the stack to be used for every INT 15h E820h
    ; calls.
    sub     esp, BCP_SIZE
    mov     BYTE [esp + BCP_IN_INT_OFF], 0x15
    mov     DWORD [esp + BCP_INOUT_EBX_OFF], 0x0
    mov     DWORD [esp + BCP_INOUT_ECX_OFF], 24
    mov     DWORD [esp + BCP_INOUT_EDX_OFF], 0x534D4150
    mov     [esp + BCP_INOUT_EDI_OFF], ebx

    ; Call the memory map function as long as we haven't reached the last entry
    ; or got an error back.
.loopEntries:
    mov     DWORD [esp + BCP_INOUT_EAX_OFF], 0xe820

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

    ; The output EBX contains the value of the next signature for the next call
    ; to the BIOS function, hence we don't need to write it ourselves to the BCP
    ; struct.
    ; An output continuation of 0x0 means this was the last entry in the map.
    mov     eax, [esp + BCP_INOUT_EBX_OFF]
    test    eax, eax
    jnz     .loopEntries
.done:

    leave
    ret
