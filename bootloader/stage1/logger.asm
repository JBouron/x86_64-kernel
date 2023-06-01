; Utility routines to log messages from stage 1.

; Log a message.
; @param (WORD) %1: Pointer to the format string.
; @param (WORD) %2..%n: Values for the format string, note that those tokens are
; pushed as is on the stack when calling printf.
%macro LOG 1-*
    jmp     %%cont
    ; Allocate the string in the middle of the code. This is a bit dirty but we
    ; don't have much of a choice since we are not using section atm.
    %%str:
    DB  %1, `\n`, `\0`
    %%cont:
    %rep    (%0 - 1)
    %rotate -1
    push    %1
    %endrep
    push    %%str
    call    printf
    add     esp, 0x4 * %0
%endmacro

BITS    32

VGA_BUFFER_ROWS EQU 25
VGA_BUFFER_COLS EQU 80
VGA_BACKGROUND_COLOR EQU 0
VGA_FOREGROUND_COLOR EQU 7
VGA_BUFFER_PHY_ADDR EQU 0xb8000

; ==============================================================================
; Clear the VGA buffer.
clearVgaBuffer:
    push    ebp
    mov     ebp, esp
    push    edi

    mov     ecx, 80 * 25
    mov     edi, VGA_BUFFER_PHY_ADDR
    xor     ax, ax
    rep stosw

    pop     edi
    leave
    ret

; ==============================================================================
; Scroll the VGA buffer up one line.
scrollVgaBufferUp:
    push    ebp
    mov     ebp, esp
    push    edi
    push    esi

    mov     ecx, VGA_BUFFER_ROWS * (VGA_BUFFER_COLS - 1)
    mov     edi, VGA_BUFFER_PHY_ADDR
    mov     esi, VGA_BUFFER_PHY_ADDR + VGA_BUFFER_COLS * 2
    rep movsw

    pop     esi
    pop     edi
    leave
    ret

; ==============================================================================
; Print a character in the VGA buffer at the current cursor position.
; @param (DWORD) c: The char to print.
_putCharVga:
    push    ebp
    mov     ebp, esp
    push    ebx

    mov     al, [ebp + 0x8]
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
    movzx   ebx, WORD [.cursorPos]
    mov     [ebx + VGA_BUFFER_PHY_ADDR], ax
    ; Advance the cursor to the next char pos in the buffer.
    add     WORD [.cursorPos], 2
    ; Fall-through to .checkIfScrollUpNeededAndOut.

.checkIfScrollUpNeededAndOut:
    ; AX = Index of buffer end.
    mov     ax, VGA_BUFFER_ROWS * VGA_BUFFER_COLS * 2
    cmp     [.cursorPos], ax
    jb      .out
    ; Need to scroll the buffer up.
    call    scrollVgaBufferUp
    ; Fixup the cursor to start on the previous line (e.g. the last line).
    sub     WORD [.cursorPos], VGA_BUFFER_COLS * 2

.out:
    pop     ebx
    leave
    ret

; The linear index of the cursor in the VGA buffer.
.cursorPos:
DW  0x0

; ==============================================================================
; Print a character.
; @param (WORD) c: The char to print.
_putChar:
    jmp     _putCharVga

; ==============================================================================
; Helper function for printf, print a value in hexadecimal format.
; @parma (DWORD) val: The value to be printed.
printfSubstitute:
    push    ebp
    mov     ebp, esp
    push    esi

    ; First print the "0x" prefix.
    mov     al, `0`
    push    eax
    call    _putChar
    mov     al, `x`
    push    eax
    call    _putChar
    add     esp, 0x8

    ; Now print the value in hex format.
    ; SI = value
    mov     esi, [ebp + 0x8]

    ; [EBP - 0x8] = Mask
    ; This mask is used to extract the 4-bit hex digits from the value.
    push    0xf0000000
    ; [EBP - 0xc] = Shift
    ; Used to right shift the bits extracted with Mask.
    push    28
.loop:
    cmp     DWORD [ebp - 0x8], 0
    je      .out

    ; AL = Next hex digits.
    mov     eax, esi
    and     eax, [ebp - 0x8]
    mov     ecx, [ebp - 0xc]
    shr     eax, cl

    cmp     al, 0xa
    jae     .loopPrintDigitAbove10
    ; This is a num.
    add     al, `0`
    jmp     .loopPrintChar
.loopPrintDigitAbove10:
    sub     al, 0xa
    add     al, `a`
.loopPrintChar:
    push    eax
    call    _putChar
    add     esp, 0x4

    ; Update iteration vars.
    mov     cl, 4
    shr     DWORD [bp - 0x8], cl
    sub     DWORD [bp - 0xc], 4
    jmp     .loop

.out:
    ; De-alloc iteration variables.
    add     esp, 0x8
    pop     esi
    leave
    ret

; ==============================================================================
; Print a formatted string.
; @param (WORD) str: The formatted NUL-terminated string. Each '$' character is
; interpreted as a substitution and replaced with the hexadecimal value of the
; corresponding WORD in the varargs val... (e.g stack).
; @param (WORD) val...: The values to be printed.
printf:
    push    ebp
    mov     ebp, esp
    push    esi
    push    ebx    

    ; ESI = Pointer to next char to print.
    mov     esi, [ebp + 0x8]

    ; EBX = Ptr to next value to substitute.
    lea     ebx, [ebp + 0xc]

    ; Loop over each char of the string until we processed the whole string. For
    ; each char:
    ;   * If the char is a regular char print it.
    ;   * If this is a substitution sequence, print the next value.
.charLoop:
    ; Loop condition, check for NUL char.
    cmp     BYTE [esi], 0
    je      .out

    ; Check if this is a substitution char.
    cmp     BYTE [esi], `$`
    jne     .charLoopRegular
    ; This is a substitution char, print the value.
    push    DWORD [ebx]
    call    printfSubstitute
    add     esp, 0x4
    add     ebx, 0x4
    jmp     .charLoopTail

.charLoopRegular:
    mov     al, [esi]
    push    eax
    call    _putChar
    add     esp, 0x4

.charLoopTail:
    inc     esi
    jmp     .charLoop

.out:
    pop     ebx
    pop     esi
    leave
    ret
