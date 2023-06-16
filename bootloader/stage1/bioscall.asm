; Helper functions to call BIOS functions from 32-bit Protected-Mode.

%include "macros.mac"
%include "bioscallconsts.inc"
%include "pm.inc"
%include "lm.inc"

; Mask of the Protected-Enable (PE) bit in cr0.
CR0_PE_BIT_MASK EQU 1

SECTION .text

BITS    32

; ==============================================================================
; Jump back to real-mode from 32-bit protected-mode. After re-enabling real-mode
; and jumping to the real-mode target the state of the the callee-saved
; registers are identical to what they were prior to calling this function. The
; stack will be pointing at the return address for this function (which is 4
; bytes long!).
; @param (WORD) jumpTarget: The address the execution should branch to after
; enabling real-mode. The 
; Note: THIS FUNCTION DOES NOT RETURN!
_jumpToRealMode:
    ; DX = jump target.
    mov     dx, [esp + 0x4]

    ; Ah, the good old switcharoo back to real-mode. Doing so is not that
    ; complicated but Intel's documentation is not that great on the subject
    ; (they don't even mention the need for getting back to 16-bit protected
    ; mode, e.g. step #1 below) and AMD does not even touch the subject.
    ; Here are the steps to switch back to real mode:
    ;   1. Jump to a 16-bit code segment (e.g. still in protected mode!).
    ;   2. Set the other segment registers to 16-bit data segments. This will be
    ;   needed for step#3 (I think we can get away with just setting DS here as
    ;   long as we don't use the stack).
    ;   3. Restore the real mode IDTR.
    ;   4. Disable protected mode by setting the CRO.PE bit to zero.
    ;   5. Immediately after setting the bit to zero perform a long jump into
    ;   real-mode.

    ; Step 1: Jump to 16-bit code segment.
    jmp     SEG_SEL(3):.16bitCodeSeg

BITS    16
.16bitCodeSeg:
    ; Step 2: Set the other segment registers to 16-bit data segment.
    mov     ax, SEG_SEL(4)
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Step 3: Restore the real mode IDT.
    ; The real mode IDT was saved when first enabling 32-bit protected-mode
    ; upong stage 1 entry.
    lidt    [realModeIdtr]

    ; Step 4: Disable protected mode.
    mov     eax, cr0
    and     eax, ~CR0_PE_BIT_MASK
    mov     cr0, eax

    ; Step 5: Jump to real mode.
    ; Per the doc's this jump *must* come right after the mov to cr0.
    jmp     0x0000:.realModeTarget
    
.realModeTarget:
    ; We are now back in real mode.

    ; Set the other segment registers to real-mode segments.
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Jump to the requested target, which was saved to DX at the beginning of
    ; the function.
    jmp     dx

CR0_PG_BIT_MASK         EQU (1 << 31)
IA32_EFER_MSR           EQU 0xC0000080
IA32_EFER_LME_BIT_MASK  EQU (1 << 8)

BITS    64
; ==============================================================================
; Jump back to 32-bit protected mode from 64-bit long-mode.
; @param %RDI: Address to jump to after enabling back 32-bit protected mode.
; THIS FUNCTION DOES NOT RETURN.
_jumpToProtectedMode:
    ; Jumping back to protected mode consists of 5 steps.
    ; Step 1: Switch to compatibility mode in CPL=0.
    sub     rsp, 6
    mov     WORD [rsp + 0x4], SEG_SEL(1)
    mov     DWORD [rsp], .compatMode
    jmp     FAR DWORD [rsp]

BITS    32
.compatMode:
    ; Clean-up the far pointer created on the stack.
    add     esp, 6

    ; Step 2: Deactivate long-mode by disabling paging.
    mov     eax, cr0
    and     eax, ~(1 << 31)
    mov     cr0, eax

    ; Step 3: Load CR3 with a legacy page-table.
    ; This is skipped in our case as we are not using paging in 32-bit mode.

    ; Step 4: Disable long mode by clearing EFER.LME.
    mov     ecx, IA32_EFER_MSR
    rdmsr
    and     eax, ~IA32_EFER_LME_BIT_MASK
    wrmsr

    ; Step 5: Enable legacy paging by setting CR0.PG.
    ; Again skipped in our case, no paging in 32-bit for us.

    ; We are now back in 32-bit protected mode, jump to the given address.
    jmp     edi

; ==============================================================================
; Call a BIOS function from 64-bit long-mode. This function takes care of
; jumping all the way back to real-mode, execute the BIOS function, return to
; 64-bit long-mode and return the result of the call. All mode switching is
; transparent to the caller.
; Caller to this function is passing a pointer to a BIOSCallPacket structure
; which is used to both pass the arguments for the BIOS call and the results
; (arguments and results here being the values of the GP registers before and
; after the call respectively). See bioscallconst.inc for the offset and size of
; each field in a BIOSCallPacket (BCP) structure.
; @param %RDI bcpAddr: Pointer to a BCP packet. This address must be <65k since
; it needs to be accessed from real-mode (real-mode uses 0x0000 data segment).
; The struct is modified in-place, indicating the value of each register after
; the BIOS function returned.
BITS    64
DEF_GLOBAL_FUNC(callBiosFunc):
    push    rbp
    mov     rbp, rsp
    push    rbx

    ; RBX = BCP packet addr.
    mov     rbx, rdi

    ; Check that the BCP packet will be accessible from real-mode.
    test    rbx, 0xffffffffffff0000
    jz      .bcpPacketAccessible
    ; BCP packet is above the 65k limit.
    PANIC   "ERROR: BCP packet is above the 65k limit ($)", rbx
.bcpPacketAccessible:

    ; Jump to protected-mode. The BX value will not changed during this
    ; operation and thus will still point to the BCP packet once in real-mode.
    mov     rdi, .protectedModeTarget
    jmp     _jumpToProtectedMode

BITS    32
.protectedModeTarget:
    ; Since we used a jmp and not a call for _jumpToProtectedMode, we don't need
    ; to fix-up the stack.

    ; Jump to real-mode, the BX value will not changed during this operation and
    ; thus will still point to the BCP packet once in real-mode.
    push    .realModeTarget
    call    _jumpToRealMode

BITS    16
.realModeTarget:
    ; Back in real mode now.

    ; Save all the register prior to calling the BIOS function. We don't trust
    ; the BIOS functions not to clobber callee-saved registers. We could get
    ; away with just saving the callee-saved registers but it is less error
    ; prone to use pusha/popa rather than multiple pushes/pops
    pushad

    ; Write the immediate value in the `INT imm8` below.
    mov     al, [bx + BCP_IN_INT_OFF]
    mov     [.intNum], al

    ; Set the value of the inputs.
    mov     eax, [bx + BCP_INOUT_EAX_OFF]
    mov     ecx, [bx + BCP_INOUT_ECX_OFF]
    mov     edx, [bx + BCP_INOUT_EDX_OFF]
    mov     edi, [bx + BCP_INOUT_EDI_OFF]
    mov     esi, [bx + BCP_INOUT_ESI_OFF]
    mov     ebp, [bx + BCP_INOUT_EBP_OFF]
    ; BX comes last since it is the register holding the pointer to the BCP
    ; struct.
    mov     ebx, [bx + BCP_INOUT_EBX_OFF]
    ; Execute the interrupt/BIOS function.
    ; Opcode for INT imm8
    DB      0xcd
.intNum:
    ; Place-holder for the interrupt number, modified during runtime.
    DB      0x0

    ; We returned from the interrupt/BIOS function, we now need to save the
    ; register values in the BCP struct.
    ; First off, save the FLAGS register since it contains the CF that indicates
    ; (for most functions) if the BIOS function was successful or not.
    pushf

    ; Now write the current register value back to the BCP struct, we need to be
    ; careful not to clobber any register prior to writting it. We only need one
    ; register to perform the writes. Use BX.
    push    ebx

    ; Recover the pointer to the BCP struct by using the saved EBX pushed on the
    ; stack during the `pusha` above. This saved value is at offset:
    ;   SP + 4 + 2 + 4 * 4
    ;        |   |    \ /
    ;        |   |     `-- pushad (4 registers in front of EBX)
    ;        |   `-------- pushf
    ;        `------------ push ebx
    ; BX = Pointer to BCP struct.
    mov     bx, sp
    mov     bx, [bx + 4 + 2 + 4*4]

    ; Write the output values to the BCP struct.
    mov     [bx + BCP_INOUT_EAX_OFF], eax
    mov     [bx + BCP_INOUT_ECX_OFF], ecx
    mov     [bx + BCP_INOUT_EDX_OFF], edx
    mov     [bx + BCP_INOUT_EDI_OFF], edi
    mov     [bx + BCP_INOUT_ESI_OFF], esi
    mov     [bx + BCP_INOUT_EBP_OFF], ebp
    ; The last missing value is that of BX itself, which was saved on the stack.
    pop     DWORD [bx + BCP_INOUT_EBX_OFF]

    ; Save the CF value in the BCP.
    popf
    setc    [bx + BCP_OUT_JC]

    ; All output values have been written back to the BCP. Restore the registers
    ; and return to 32-bit protected mode.
    popad

    push    .retToProtectedMode
    call    jumpToProtectedMode

BITS    32
.retToProtectedMode:
    ; Now back to protected mode. We still need to fix-up the stack which at
    ; this point contains:
    ;   |                       .......                            |   ...
    ;   | param of _jumpToRealMode call                            | ESP + 8
    ;   | ret addr for _jumpToRealMode call                        | ESP + 4
    ;   | param jumpToProtectedMode | ret addr jumpToProtectedMode | <- ESP
    ; Notice that because the call to jumpToProtectedMode was executed in
    ; real-mode, the parameter and return address for this call are 2 bytes each
    ; instead of 4.
    ; By adding 0xc back to ESP we get the ESP we had right before calling
    ; _jumpToRealMode.
    add     esp, 0xc

    push    .retToLongMode
    call     jumpToLongMode

BITS    64
.retToLongMode:
    ; Fixup the stack again to remote the param of jumpToLongMode. Since we jmp
    ; and not call'ed jumpToLongMode there is no return address to clean-up from
    ; the stack.

    ; We can now safely return.
    pop     rbx
    leave
    ret
