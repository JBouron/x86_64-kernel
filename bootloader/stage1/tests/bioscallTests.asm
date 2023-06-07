; Tests for the bioscall functionality.

%include "macros.mac"
%include "bioscall.inc"

SECTION .text

; Helper macro to lock up the CPU in case of a test failure.
; @param %1: Pointer to message string to be logged before the lock-up.
%macro TEST_FAIL 1
    LOG %1
    %%testFailed:
    cli
    jmp     %%testFailed
%endmacro

EXT_VAR(realModeIdtrBase)

BITS    32

; ==============================================================================
; End-to-end test for callBiosFunc. This test makes sure that the right BIOS
; function is called with the right arguments and that the BCP struct is
; correctly updated to contain the output register values.
DEF_GLOBAL_FUNC(callBiosFuncTest):
    push    ebp
    mov     ebp, esp

    ; Prepare a mock BIOS function to be used by the test. This function
    ; computes the 1 complement of each register and set CF to 1. This function
    ; is invoked using interrupt #7. Since we don't want to clobber the real
    ; real-mode IDT (it might be used after the tests) make sure to backup the
    ; associated entry.
    ; Backup the entry.
    mov     eax, [realModeIdtrBase]
    mov     ecx, [eax + 7 * 4]
    mov     [ivtEntryBackup], ecx
    ; Register the mock BIOS function. In real mode an IVT entry is simply a 4
    ; bytes far pointer CS:Offset (hence offset in lower bits).
    mov     ecx, _mockBiosFunction
    ; Double check that the address of the mockBiosFunction fits in real-mode
    ; segment 0x0000.
    test    ecx, 0xffff0000
    jz      .ok
    TEST_FAIL   "Cannot register mock BIOS function, offset too high"
.ok:
    mov     [eax + 7 * 4], ecx

    ; Now that the mock function is ready, prepare a BCP struct. All registers
    ; get a peculiar value.
    sub     esp, BCP_SIZE
    mov     DWORD [esp + BCP_IN_INT_OFF], 0x7
    mov     DWORD [esp + BCP_INOUT_EAX_OFF], 0xABCDEF01
    mov     DWORD [esp + BCP_INOUT_EBX_OFF], 0xBCDEF012
    mov     DWORD [esp + BCP_INOUT_ECX_OFF], 0xCDEF0123
    mov     DWORD [esp + BCP_INOUT_EDX_OFF], 0xDEF01234
    mov     DWORD [esp + BCP_INOUT_EDI_OFF], 0xEF012345
    mov     DWORD [esp + BCP_INOUT_ESI_OFF], 0xF0123456
    mov     DWORD [esp + BCP_INOUT_EBP_OFF], 0x01234567

    ; Call the bios function.
    push    esp
    call    callBiosFunc
    add     esp, 0x4

    ; Now check that the value of each register in the BCP struct as been
    ; updated as expected.
    cmp     BYTE [esp + BCP_OUT_JC], 0x1
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_EAX_OFF], ~0xABCDEF01
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_EBX_OFF], ~0xBCDEF012
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_ECX_OFF], ~0xCDEF0123
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_EDX_OFF], ~0xDEF01234
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_EDI_OFF], ~0xEF012345
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_ESI_OFF], ~0xF0123456
    jne     .invalidOutValue
    cmp     DWORD [esp + BCP_INOUT_EBP_OFF], ~0x01234567
    jne     .invalidOutValue

    ; All values are as expected. Restore the IVT entry we used for the mock
    ; function and return.
    mov     eax, [realModeIdtrBase]
    mov     ecx, [ivtEntryBackup]
    mov     [eax + 7 * 4], ecx

    xor     eax, eax
    leave
    ret
.invalidOutValue:
    TEST_FAIL   "Invalid output value in BCP struct"
    ; Unreachable
    int3

BITS    16
_mockBiosFunction:
    ; The custom BIOS function used for the callBiosFuncTest test. This function
    ; simply computes the 1-complement of all registers.
    not     eax
    not     ebx
    not     ecx
    not     edx
    not     edi
    not     esi
    not     ebp
    ; Set the CF in the saved FLAGS on the stack. This saved flag is at offset
    ; +4 from SP at this point. However, [sp + imm8] is not a valid addressing
    ; in real-mode so we need to use a temp register and make sure not to
    ; clobber it.
    push    bx
    mov     bx, sp
    or      WORD [bx + 6], 1
    pop     bx
    ; Now return to the INT instruction.
    iret
BITS    32

SECTION .data

; Backup of the IVT entry used to register the custom BIOS function.
ivtEntryBackup:
DD  0x0
