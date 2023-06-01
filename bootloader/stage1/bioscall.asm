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
