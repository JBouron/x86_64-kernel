; Entry-point of the bootloader once BIOS reliquishes control. We are running in
; good old real-mode here.

BITS 16

; Helper macro to log a string in the VGA buffer. Calls printString on the given
; arg.
; @param %1: label containing a NUL terminated string.
%MACRO PRINT_STRING 1
    push    ax
    lea     ax, [%1]
    push    ax
    call    printString
    add     sp, 0x2
    pop     ax
%ENDMACRO

; Put the origin to 0x7c00. The goal here is to use zero segment registers
; because it makes it easier to reason about and compare addresses.
ORG 0x7c00

entry:
    ; Interrupts are usually disabled by the BIOS. Doesn't hurt to disable them
    ; ourselves here just in case.
    cli

    ; The code is being assembled expecting to be loaded at address 0x7c00 (see
    ; the ORG directive above). Not all BIOS set CS to 0x0 when relinquishing
    ; control to the bootloader, some set CS=0x07c0 and IP=0x0, hence make sure
    ; the CS and IP are set as expected.
    jmp     0x0:.entryLongJumpTarget
.entryLongJumpTarget:
    ; Reset all segment registers to use 0x0.
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; Save the index of the boot drive, the BIOS passes this index in DL.
    mov     [data.bootDriveIndex], dl

    ; Setup a small stack right below the start of the bootloader. This is a bit
    ; unusual compared to typical address spaces where stacks are in high
    ; addresses and code in low addresses. However in our situation, setting the
    ; stack to a low address means we can keep using SS=0x0 which is easier to
    ; deal with, especially once we will enable paging etc...
    lea     sp, [entry]

    ; We are live!
    PRINT_STRING data.bootMessage

.dead:
    hlt
    jmp     .dead


; Print a NUL-terminated string into the VGA buffer.
; @param (WORD) stringAddr: The address of the NUL terminated string to print.
printString:
    push    bp
    mov     bp, sp
    push    di
    push    bx

    ; First get the cursor position.
    mov     ah, 0x3
    mov     bh, 0x0
    ; DH = row, DL = col. DX won't change until we call the printing function,
    ; which also expects the row and col in DH and DL.
    int     0x10

    ; Count the number of chars in the string as the BIOS function needs this
    ; info to print it.
    mov     di, [bp + 0x4]
    xor     al, al
    mov     cx, -1
    repne scasb
    test    cx, cx
    ; CX can only be zero if the string is more than 65k long, which is
    ; virtually impossible.
    jz      .out
    ; DI = length of string. Notice the additional -1 needed because the repne
    ; scasb above still increment the di reg even when the NUL char is found.
    sub     di, [bp + 0x4]
    dec     di

    ; The BIOS function uses BP as a parameter.
    push    bp

    ; Update cursor after writing, regular string (e.g. no in-band attrs).
    mov     ax, 0x1301
    ; Page 0 and use gray foreground on black background.
    mov     bx, 0x0007
    mov     cx, di
    ; DH and DL are still containing the cursor position where we want to print.
    mov     bp, [bp + 0x4]
    int     0x10

    pop     bp
.out:
    pop     bx
    pop     di
    leave
    ret

; The data ""section"".
data:
; The index of the drive we loaded from.
.bootDriveIndex:
DB  0x0

; Some debug messages.
.bootMessage:
DB  `== Jumped into stage 0 entry point ==\n\r\0`

; Insert zeros until offset 510.
TIMES 510-($-$$) DB 0

; Boot signature.
DB  0x55
DB  0xaa
