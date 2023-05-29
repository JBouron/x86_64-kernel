; Entry-point of the bootloader once BIOS reliquishes control. We are running in
; good old real-mode here.

BITS 16

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

    ; Setup a small stack right below the start of the bootloader. This is a bit
    ; unusual compared to typical address spaces where stacks are in high
    ; addresses and code in low addresses. However in our situation, setting the
    ; stack to a low address means we can keep using SS=0x0 which is easier to
    ; deal with, especially once we will enable paging etc...
    lea     sp, [entry]

    ; Clear the VGA screen.
    mov     ax, 0xB800
    mov     es, ax
    xor     ax, ax
    mov     cx, 80 * 25
    xor     di, di
    rep stosb

    ; Print message.
    mov     ax, 0xB800
    mov     es, ax
    xor     di, di
    mov     WORD [es:di + 0x0], ((7 << 8) | 'B')
    mov     WORD [es:di + 0x2], ((7 << 8) | 'o')
    mov     WORD [es:di + 0x4], ((7 << 8) | 'o')
    mov     WORD [es:di + 0x6], ((7 << 8) | 't')
    mov     WORD [es:di + 0x8], ((7 << 8) | 'e')
    mov     WORD [es:di + 0xa], ((7 << 8) | 'd')

.dead:
    hlt
    jmp     .dead

; Insert zeros until offset 510.
TIMES 510-($-$$) DB 0

; Boot signature.
DB  0x55
DB  0xaa
