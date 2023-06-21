; Entry-point of the bootloader once BIOS reliquishes control. We are running in
; good old real-mode here.
;
; Goals
; ======
; Stage 0 is responsible to carry-out the following tasks:
;   * Setup sane segment registers, more specifically it sets all segments
;   registers to 0x0000. This makes it easier to reason about address and also
;   comes in handy once we switch to protected mode as the offsets remain the
;   same.
;   * Setup the stack.
;   * Make sure the A20 line has been enabled.
;   * Load and jump to stage 1. At the time of the jump, DL contains the index
;   of the boot drive.
;
; Assumptions
; ===========
;   * For now this stage only checks that A20 line has been enabled by the BIOS
;   but does not enable it itself if that is not the case. This should be fine
;   however as virtually all BIOSes are enabling the A20 line these days.
;   * When loading the next stage, this stage expects to see a header on in the
;   very first sector of the next stage starting at offset 0. This header is as
;   follows:
;       Offset      Size    Description
;       ------------------------------------------------------------------------
;       0x0         0x2     The signature 0x50f3. This is how this stage
;                           recognizes a valid next stage.
;       0x2         0x2     The size of the next stage in units of sectors.
;                           This indicates how many sectors should be loaded
;                           from disk before jumping to the entry point of the
;                           next stage. Note: This includes the first sector
;                           containing the header!
;       0x4         0x2     The offset of the entry point of the next stage,
;                           relative to the physical address 0x7e00.
;   * This stage loads the next stage at address 0000:7e00.

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

; ==============================================================================
; Entry point of stage 0. This is the first code executed when the BIOS
; reliquishes control of the cpu.
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

    ; Check if the BIOS enabled the A20 line.
    ; FIXME: For now we rely on the BIOS to enable the A20 line for us because
    ; frankly this seem a bit annoying to do this ourselves. These days
    ; virtually all BIOSes do that so in practice this should not be a problem.
    call    isA20LineEnabled
    test    ax, ax
    jnz     .a20Enabled
    ; A20 not enabled, log message and die.
    PRINT_STRING data.a20NotEnabledMessage
    jmp     .dead
.a20Enabled:

    ; Load the first sector of the next stage.
    ; Load on the next sector boundary. We do this because the next stage is
    ; assembled at origin = 0x7e00. We are wasting a few bytes this way, but
    ; this is clearly making things easier as the next stage does not have to
    ; know exactly how big this stage is (e.g. to compute the correct origin).
    push    0x7e00
    push    1
    push    1
    xor     dh, dh
    mov     dl, [data.bootDriveIndex]
    push    dx
    call    loadSectors
    add     sp, 0x8
    test    ax, ax
    jnz     .firstSectorRead
    ; Failed to read the first sector of the next stage.
    PRINT_STRING data.loadNextStageFirstSectorFailedMessage
    jmp     .dead
.firstSectorRead:
    ; First sector was successfully loaded to memory.

    ; Check the magic number.
    mov     bx, 0x7e00
    cmp     WORD [bx], 0x50f3
    je      .magicNumberOk
    ; The magic number does not match.
    PRINT_STRING data.invalidNextStageMagicNumberMessage
    jmp     .dead
.magicNumberOk:

    ; Load the remaining sectors.
    ; AX = Number of sectors to load. -1 because we already loaded the first
    ; sector.
    mov     ax, [bx + 0x02]
    dec     ax
    test    ax, ax
    jz      .remainingSectorsLoaded

    ; There are more sectors to load. Start reading from sector #2.
    push    0x7e00 + 512
    push    ax
    push    2
    xor     dh, dh
    mov     dl, [data.bootDriveIndex]
    push    dx
    call    loadSectors
    add     sp, 0x8
    test    ax, ax
    jnz     .remainingSectorsLoaded
    ; Failed to load remaining sectors.
    PRINT_STRING data.loadNextStageRemainingSectorsFailedMessage
    jmp     .dead
.remainingSectorsLoaded:

    ; Jump to stage 1's entry.
    ; Set DL to boot drive index.
    mov     dl, [data.bootDriveIndex]
    mov     bx, [bx + 0x4]
    lea     ax, [0x7e00 + bx]
    jmp     ax

.dead:
    hlt
    jmp     .dead

; ==============================================================================
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

; ==============================================================================
; Check if the A20 line has been enabled.
; @return (AX): If enabled return AX=1, else return AX=0.
isA20LineEnabled:
    ; The easiest way to do check if A20 is enabled is to try to read at an
    ; address that overflows. We do this with the boot signature located at
    ; 0000:7dfe which is aliased to ffff:7e0e (which has its bit 20 to 1). If
    ; reading from both addresses give different values then there are good
    ; chances the A20 is already enabled. To make sure this is not a
    ; coincidence, read a first time, compare, modify the value and then read a
    ; second time.

    ; Use ES segment to read the overflow address.
    push    es
    mov     ax, 0xffff
    mov     es, ax

    mov     ax, 0x1

    ; First try.
    mov     cx, [es:0x7e0e]
    cmp     cx, [0x7dfe]
    jne     .out
    ; The value matches, modify the address 0000:7dfe and re-try to make sure.
    not     WORD [0x7dfe]
    ; Second try.
    mov     cx, [es:0x7e0e]
    cmp     cx, [0x7dfe]
    jne     .out
    ; The value matched again, we can say with almost certainty that the BIOS
    ; has not enabled the A20 line for us.
    xor     ax, ax
.out:
    ; Restore ES.
    pop     es
    ; While not necessary, restore the boot signature.
    mov     WORD [0x7dfe], 0xaa55
    ret

; ==============================================================================
; Load sectors from the specified disk using BIOS function INT 13h AH=42h.
; @param (WORD) driveIndex: The index of the drive to read from.
; @param (WORD) startSectorIndex: LBA index of the first sector to read.
; @param (WORD) numSectors: Number of sectors to read starting at
; `startSectorIndex`.
; @param (WORD) addr: Where to write the sectors content in memory. The actual
; address is DS:addr.
; @return (AX): If successful, AX=1, otherwise AX=0.
loadSectors:
    push    bp
    mov     bp, sp

    push    si

    ; Prepare Disk Address Packet on the stack, this packet has the following
    ; layout:
    ;   Offset  Size    Desc
    ;   0x0     0x1     Size of the packet (e.g. 0x10).
    ;   0x1     0x1     Must be zero.
    ;   0x2-0x3 0x2     Number of sectors to be read.
    ;   0x4-0x7 0x4     Seg:Offset ptr for dest. buffer. Offset first.
    ;   0x8-0xf 0x8     LBA index of first sector to read.
    ; LBA index of first sector.
    push    0x0
    push    0x0
    push    0x0
    push    WORD [bp + 0x6]
    ; Dest buffer.
    push    0x0
    push    WORD [bp + 0xa]
    ; Number of sectors to be read.
    push    WORD [bp + 0x8]
    ; Zero + packet size
    push    0x10

    ; Do the BIOS call.
    mov     ah, 0x42
    mov     dl, [bp + 0x4]
    mov     si, sp
    int     0x13

    setnc    al
    xor     ah, ah

    ; De-allocate the disk address packet from the stack.
    add     sp, 0x10

    pop     si
    leave
    ret

; The data ""section"".
data:
; The index of the drive we loaded from.
.bootDriveIndex:
DB  0x0

; Some debug messages.
.a20NotEnabledMessage:
DB  `A20 line not enabled\n\r\0`
.loadNextStageFirstSectorFailedMessage:
DB  `Failed to load next stage's first sector from disk\n\r\0`
.invalidNextStageMagicNumberMessage:
DB  `Invalid magic number in next sector\n\r\0`
.loadNextStageRemainingSectorsFailedMessage:
DB  `Failed to load remaining sectors\n\r\0`

; Insert zeros until offset 510.
TIMES 510-($-$$) DB 0

; Boot signature.
DB  0x55
DB  0xaa
