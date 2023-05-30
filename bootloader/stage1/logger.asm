; Utility routines to log messages from stage 1.

BITS    16

VGA_BUFFER_ROWS EQU 25
VGA_BUFFER_COLS EQU 80
VGA_BACKGROUND_COLOR EQU 0
VGA_FOREGROUND_COLOR EQU 7

; ==============================================================================
; Clear the VGA buffer.
clearVgaBuffer:
    push    bp
    mov     bp, sp
    push    es
    push    di

    mov     ax, 0xb800
    mov     es, ax
    mov     cx, 80 * 25
    xor     di, di
    xor     ax, ax
    rep stosw

    pop     di
    pop     es
    leave
    ret

; ==============================================================================
; Scroll the VGA buffer up one line.
scrollVgaBufferUp:
    push    bp
    mov     bp, sp
    push    ds
    push    es
    push    di
    push    si

    mov     ax, 0xb800
    mov     es, ax
    mov     ds, ax
    mov     cx, VGA_BUFFER_ROWS * (VGA_BUFFER_COLS - 1)
    xor     di, di
    mov     si, VGA_BUFFER_COLS * 2
    rep movsw

    pop     si
    pop     di
    pop     es
    pop     ds
    leave
    ret

; ==============================================================================
; Print a character in the VGA buffer at the current cursor position.
; @param (WORD) c: The char to print.
_putCharVga:
    push    bp
    mov     bp, sp
    push    es
    push    bx

    mov     ax, 0xb800
    mov     es, ax

    mov     ax, [bp + 0x4]
    cmp     al, `\n`
    je      .handleNewLine
    jmp     .handleRegularChar

.handleNewLine:
    ; For new line we simply have to update the index of cursor to start to the
    ; next line, e.g. the next multiple of VGA_BUFFER_COLS*2. The *2 here is
    ; because the VGA buffer is an array of WORDs.
    mov     ax, [.cursorPos]
    mov     cl, VGA_BUFFER_COLS*2
    ; AH = cursorPos % (VGA_BUFFER_COLS * 2)
    div     cl
    ; If the cursorPos is already on the beginning of a line there is no need to
    ; go to the next line. This makes it a bit prettier.
    test    ah, ah
    jz      .checkIfScrollUpNeededAndOut
    ; Otherwise we need to fix-up the cursorPos.
    movzx   ax, ah
    neg     ax
    add     ax, VGA_BUFFER_COLS*2
    add     [.cursorPos], ax
    jmp     .checkIfScrollUpNeededAndOut

.handleRegularChar:
    ; For regular characters we simply print on the current cursorPos and
    ; increment the cursorPos.
    ; At this point AL already contains the character to be printed, we only
    ; need to modify AH to contain the proper color attributes.
    mov     ah, (VGA_BACKGROUND_COLOR << 4) | VGA_FOREGROUND_COLOR
    mov     bx, [.cursorPos]
    mov     [es:bx], ax
    ; Advance the cursor to the next char pos in the buffer.
    add     WORD [.cursorPos], 2
    ; Fall-through to .checkIfScrollUpNeededAndOut.

.checkIfScrollUpNeededAndOut:
    ; AX = Buffer end.
    mov     ax, VGA_BUFFER_ROWS * VGA_BUFFER_COLS * 2
    cmp     [.cursorPos], ax
    jb      .out
    ; Need to scroll the buffer up.
    call    scrollVgaBufferUp
    ; Fixup the cursor to start on the previous line (e.g. the last line).
    sub     WORD [.cursorPos], VGA_BUFFER_COLS * 2

.out:
    pop     bx
    pop     es
    leave
    ret

; The linear index of the cursor in the VGA buffer.
.cursorPos:
dw  0x0

; ==============================================================================
; Print a character.
; @param (WORD) c: The char to print.
_putChar:
    jmp     _putCharVga

; ==============================================================================
; Helper function for printf, print a value in hexadecimal format.
; @parma (WORD) val: The value to be printed.
printfSubstitute:
    push    bp
    mov     bp, sp
    push    si

    ; First print the "0x" prefix.
    mov     al, `0`
    movzx   ax, al
    push    ax
    call    _putChar
    add     sp, 0x2
    mov     al, `x`
    movzx   ax, al
    push    ax
    call    _putChar
    add     sp, 0x2

    ; Now print the value in hex format.
    ; SI = value
    mov     si, [bp + 0x4]

    ; [BP - 0x4] = Mask
    ; This mask is used to extract the 4-bit hex digits from the value.
    push    0xf000
    ; [BP - 0x6] = Shift
    ; Used to right shift the bits extracted with Mask.
    push    12
.loop:
    test    WORD [bp - 0x4], -1
    jz      .out

    ; AX = Next hex digits.
    mov     ax, si
    and     ax, [bp - 0x4]
    mov     cx, [bp - 0x6]
    shr     ax, cl

    cmp     al, 0xa
    jae     .loopPrintDigitAbove10
    ; This is a num.
    add     al, `0`
    jmp     .loopPrintChar
.loopPrintDigitAbove10:
    sub     al, 0xa
    add     al, `a`
.loopPrintChar:
    movzx   ax, al
    push    ax
    call    _putChar
    add     sp, 0x2

    ; Update iteration vars.
    mov     cl, 4
    shr     WORD [bp - 0x4], cl
    sub     WORD [bp - 0x6], 4
    jmp     .loop

.out:
    ; De-alloc iteration count.
    add     sp, 0x2
    pop     si
    leave
    ret

; ==============================================================================
; Print a formatted string.
; @param (WORD) str: The formatted NUL-terminated string. Each '$' character is
; interpreted as a substitution and replaced with the hexadecimal value of the
; corresponding WORD in the varargs val... (e.g stack).
; @param (WORD) val...: The values to be printed.
printf:
    push    bp
    mov     bp, sp
    push    si
    push    bx    

    ; SI = Pointer to next char to print.
    mov     si, [bp + 0x4]

    ; BX = Ptr to next value to substitute.
    lea     bx, [bp + 0x6]

    ; Loop over each char of the string until we processed the whole string. For
    ; each char:
    ;   * If the char is a regular char print it.
    ;   * If this is a substitution sequence, print the next value.
.charLoop:
    ; Loop condition, check for NUL char.
    test    BYTE [si], -1
    jz      .out

    ; Check if this is a substitution char.
    cmp     BYTE [si], `$`
    jne     .charLoopRegular
    ; This is a substitution char, print the value.
    push    WORD [bx]
    call    printfSubstitute
    add     bx, 0x2
    jmp     .charLoopTail

.charLoopRegular:
    mov     al, [si]
    movzx   ax, al
    push    ax
    call    _putChar
    add     sp, 0x2

.charLoopTail:
    inc     si
    jmp     .charLoop

.out:
    pop     bx
    pop     si
    leave
    ret
