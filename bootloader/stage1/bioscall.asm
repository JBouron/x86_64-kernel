; Helper functions to call BIOS functions from 32-bit Protected-Mode.

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
    lidt    [data.realModeIdtr]

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

; Make sure to revert to 32-bit code here so that a function below this one does
; not un-expectedly get assembled in 16-bit.
BITS    32

; Field offsets for a BIOSCallPacket (BCP) structure. Fields that are only used
; as input are named BCP_IN_*, those only used as output are named BCP_OUT_* and
; those which are used for both are named BCP_INOUT_*.
BCP_IN_INT_OFF      EQU 0x0 ; Size: 1 Desc: Interrupt number.
BCP_OUT_JC          EQU 0x1 ; Size: 1 Desc: Output value of CF in FLAGS reg.
BCP_INOUT_AX_OFF    EQU 0x2 ; Size: 2 Desc: Input/output value for AX.
BCP_INOUT_BX_OFF    EQU 0x4 ; Size: 2 Desc: Input/output value for BX.
BCP_INOUT_CX_OFF    EQU 0x6 ; Size: 2 Desc: Input/output value for CX.
BCP_INOUT_DX_OFF    EQU 0x8 ; Size: 2 Desc: Input/output value for DX.
BCP_INOUT_DI_OFF    EQU 0xa ; Size: 2 Desc: Input/output value for DI.
BCP_INOUT_SI_OFF    EQU 0xc ; Size: 2 Desc: Input/output value for SI.
BCP_INOUT_BP_OFF    EQU 0xe ; Size: 2 Desc: Input/output value for BP.
; Size of a BCP structure.
BCP_SIZE            EQU 0x10
; For convenience the following constants provide some shortcuts to access half
; registers:
BCP_INOUT_AL_OFF    EQU BCP_INOUT_AX_OFF
BCP_INOUT_AH_OFF    EQU (BCP_INOUT_AX_OFF + 1)
BCP_INOUT_BL_OFF    EQU BCP_INOUT_BX_OFF
BCP_INOUT_BH_OFF    EQU (BCP_INOUT_BX_OFF + 1)
BCP_INOUT_CL_OFF    EQU BCP_INOUT_CX_OFF
BCP_INOUT_CH_OFF    EQU (BCP_INOUT_CX_OFF + 1)
BCP_INOUT_DL_OFF    EQU BCP_INOUT_DX_OFF
BCP_INOUT_DH_OFF    EQU (BCP_INOUT_DX_OFF + 1)

; ==============================================================================
; Call a BIOS function from 32-bit protected-mode. This function takes care of
; jumping back to real-mode, execute the BIOS function, re-enable 32-bit
; protected-mode and return the result of the call.
; Caller to this function are passing a pointer to a BIOSCallPacket structure
; which is used to both pass the arguments for the BIOS call and the results
; (arguments and results here being the values of the GP registers before and
; after the call respectively). The constants above this command indicate the
; offset and size of each field in a BIOSCallPacket (BCP) structure.
; From the point of view of the caller, this is a regular function call, the
; fact that this function goes to real-mode and back to 32-bit protected-mode is
; invisible to the caller.
; @param (DWORD) bcpAddr: Pointer to a BCP packet. This address must be <65k
; since it needs to be accessed from real-mode. The struct is modified in-place,
; indicating the value of each register after the BIOS function returned.
callBiosFunc:
    push    ebp
    mov     ebp, esp
    push    ebx

    ; EBX = BCP packet addr.
    mov     ebx, [ebp + 0x8]

    ; Check that the BCP packet will be accessible from real-mode.
    test    ebx, 0xffff0000
    jz      .bcpPacketAccessible
    ; BCP packet is above the 65k limit.
    LOG     "ERROR: BCP packet is above the 65k limit ($)", ebx
.dead:
    ; FIXME: Add a PANIC macro, function.
    hlt
    jmp     .dead
.bcpPacketAccessible:

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
    pusha

    ; Write the immediate value in the `INT imm8` below.
    mov     al, [bx + BCP_IN_INT_OFF]
    mov     [.intNum], al

    ; Set the value of the inputs.
    mov     ax, [bx + BCP_INOUT_AX_OFF]
    mov     cx, [bx + BCP_INOUT_CX_OFF]
    mov     dx, [bx + BCP_INOUT_DX_OFF]
    mov     di, [bx + BCP_INOUT_DI_OFF]
    mov     si, [bx + BCP_INOUT_SI_OFF]
    mov     bp, [bx + BCP_INOUT_BP_OFF]
    ; BX comes last since it is the register holding the pointer to the BCP
    ; struct.
    mov     bx, [bx + BCP_INOUT_BX_OFF]
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
    push    bx

    ; Recover the pointer to the BCP struct by using the saved BX pushed on the
    ; stack during the `pusha` above. This saved value is at offset:
    ;   SP + (2 + 4) * 2
    mov     bx, sp
    ; BX = Pointer to BCP struct.
    mov     bx, [bx + 12]

    ; Write the output values to the BCP struct.
    mov     [bx + BCP_INOUT_AX_OFF], ax
    mov     [bx + BCP_INOUT_CX_OFF], cx
    mov     [bx + BCP_INOUT_DX_OFF], dx
    mov     [bx + BCP_INOUT_DI_OFF], di
    mov     [bx + BCP_INOUT_SI_OFF], si
    mov     [bx + BCP_INOUT_BP_OFF], bp
    ; The last missing value is that of BX itself, which was saved on the stack.
    pop     WORD [bx + BCP_INOUT_BX_OFF]

    ; Save the CF value in the BCP.
    popf
    setc    [bx + BCP_OUT_JC]

    ; All output values have been written back to the BCP. Restore the registers
    ; and return to 32-bit protected mode.
    popa

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

    ; We can now safely return.
    pop     ebx
    leave
    ret
