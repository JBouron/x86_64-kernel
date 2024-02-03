; This file defines the startup code for the Application Processors.
; The goal here is to bring an AP from real-mode to 64-bit long-mode in a single
; routine.
;
; The "AP booting protocol", so-to-speak, is described further in
; startupApplicationProcessor().

SECTION .data
BITS    16

; Mask of the Protected-Enable (PE) bit in cr0.
CR0_PE_BIT_MASK EQU 1

CR4_PAE_BIT_MASK        EQU (1 << 5)
CR0_PG_BIT_MASK         EQU (1 << 31)
IA32_EFER_MSR           EQU 0xC0000080
IA32_EFER_LME_BIT_MASK  EQU (1 << 8)
IA32_EFER_LMA_BIT_MASK  EQU (1 << 10)

GLOBAL  apStartup:function
apStartup:
.base:
    ; We woke up from the dead. At this point, CS is set to 0x800 because this
    ; startup code is stored at physical address 0x8000. Setup all segment
    ; registers to 0x00.
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; We are force to hard-code the address of the far jump here as otherwise
    ; NASM is going to complain:
    ;   relocation truncated to fit: R_X86_64_16 against `.data'"
    ; which makes sense because this file is compiled as an ELF64.
    ; Since we know where in physical memory this code is going to be, hardcode
    ; the address so we don't trigger the error.
    jmp     0x00:(0x8000 + .rmFarJumpTarget - .base)
.rmFarJumpTarget:

    ; Setup boot stack.
    mov     sp, 0xa000 + 0x1000
    mov     bp, sp
    
    ; BX = Pointer to ApBootInfo.
    mov     bx, 0x9000

    ; Prepare the GDTR, use the temporary GDT from the ApBootInfo which is
    ; stored in the first 5 * 8 = 40 bytes of the ApBootInfo.
    lea     ax, [bx + 0x00]
    ; Construct the GDT descriptor on the stack: a 32-bit linear address (we do
    ; the zero extend ourselves) and a 16-bit limit.
    push    0x00
    push    ax
    push    40 - 1
    mov     si, sp
    lgdt    [si]
    add     sp, 6

    ; Enable Protected-mode without paging.
    mov     eax, cr0
    or      eax, CR0_PE_BIT_MASK
    mov     cr0, eax

    ; Jump into protected mode. As before the far jump needs to be hard-coded.
    jmp     0x08:(0x8000 + .pmFarJumpTarget - .base)
.pmFarJumpTarget:

    BITS    32
    ; We are now in 32-bit protected mode without paging. Initialize the other
    ; segment registers.
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Prepare long-mode activation.
    ; Enable physical-address extensions.
    mov     eax, cr4
    or      eax, CR4_PAE_BIT_MASK
    mov     cr4, eax

    ; Load CR3 with the page table address from the ApBootInfo struct.
    mov     eax, [ebx + 5 * 8]
    mov     cr3, eax

    ; Enable long-mode in EFER.
    mov     ecx, IA32_EFER_MSR
    rdmsr
    or      eax, IA32_EFER_LME_BIT_MASK
    wrmsr

    ; Enable paging, which _activates_ long-mode.
    mov     eax, cr0
    or      eax, CR0_PG_BIT_MASK
    mov     cr0, eax
    ; Per AMD's manual, the instruction following the mov to cr0 enabling paging
    ; must be a branch. Must have something to do with speculation.
    jmp     .dummy
.dummy:
    ; At this point we are in 32-bit compatibility mode.

    ; Far jump into long-mode, once again the address is hard-coded.
    jmp     0x18:(0x8000 + .lmFarJumpTarget - .base)
.lmFarJumpTarget:

    BITS    64

    ; We are now in 64-bits long mode!
    ; Finish setting up this AP and jump to the target.
    mov     rdi, rbx
    EXTERN  finalizeApplicationProcessorStartup
    ; Need to jump rax as otherwise the jump would be relative and this would
    ; not work since this code gets relocated at runtime.
    lea     rax, [finalizeApplicationProcessorStartup]
    jmp     rax

GLOBAL  apStartupEnd:function
apStartupEnd:
