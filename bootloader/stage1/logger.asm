; Utility routines to log messages from stage 1.

%include "macros.mac"

VGA_BUFFER_ROWS EQU 25
VGA_BUFFER_COLS EQU 80
VGA_BACKGROUND_COLOR EQU 0
VGA_FOREGROUND_COLOR EQU 7
VGA_BUFFER_PHY_ADDR EQU 0xb8000

SECTION .data
; The linear index of the cursor in the VGA buffer.
cursorPos:
DW  0x0

SECTION .text

BITS    32

; ==============================================================================
; Clear the VGA buffer.
DEF_GLOBAL_FUNC(clearVgaBuffer):
    push    edi

    mov     ecx, 80 * 25
    mov     edi, VGA_BUFFER_PHY_ADDR
    xor     ax, ax
    rep stosw

    pop     edi
    ret

; ==============================================================================
; Scroll the VGA buffer up one line.
scrollVgaBufferUp32:
    push    edi
    push    esi

    mov     ecx, VGA_BUFFER_ROWS * (VGA_BUFFER_COLS - 1)
    mov     edi, VGA_BUFFER_PHY_ADDR
    mov     esi, VGA_BUFFER_PHY_ADDR + VGA_BUFFER_COLS * 2
    rep movsw

    pop     esi
    pop     edi
    ret

; ==============================================================================
; Print a character in the VGA buffer at the current cursor position.
; @param (DWORD) c: The char to print.
_putCharVga32:
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
    mov     ax, [cursorPos]
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
    add     [cursorPos], ax
    jmp     .checkIfScrollUpNeededAndOut

.handleRegularChar:
    ; For regular characters we simply print on the current cursorPos and
    ; increment the cursorPos.
    ; At this point AL already contains the character to be printed, we only
    ; need to modify AH to contain the proper color attributes.
    mov     ah, (VGA_BACKGROUND_COLOR << 4) | VGA_FOREGROUND_COLOR
    movzx   ebx, WORD [cursorPos]
    mov     [ebx + VGA_BUFFER_PHY_ADDR], ax
    ; Advance the cursor to the next char pos in the buffer.
    add     WORD [cursorPos], 2
    ; Fall-through to .checkIfScrollUpNeededAndOut.

.checkIfScrollUpNeededAndOut:
    ; AX = Index of buffer end.
    mov     ax, VGA_BUFFER_ROWS * VGA_BUFFER_COLS * 2
    cmp     [cursorPos], ax
    jb      .out
    ; Need to scroll the buffer up.
    call    scrollVgaBufferUp32
    ; Fixup the cursor to start on the previous line (e.g. the last line).
    sub     WORD [cursorPos], VGA_BUFFER_COLS * 2

.out:
    pop     ebx
    leave
    ret

; ==============================================================================
; Print a character.
; @param (WORD) c: The char to print.
_putChar32:
    jmp     _putCharVga32

; ==============================================================================
; Helper function for printf, print a value in hexadecimal format.
; @parma (DWORD) val: The value to be printed.
printfSubstitute32:
    push    ebp
    mov     ebp, esp
    push    esi

    ; First print the "0x" prefix.
    mov     al, `0`
    push    eax
    call    _putChar32
    mov     al, `x`
    push    eax
    call    _putChar32
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
    call    _putChar32
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
; NEVER CALL THIS FUNCTION DIRECTLY, ALWAYS USE `LOG` MACRO.
; Print a formatted string.
; @param (WORD) str: The formatted NUL-terminated string. Each '$' character is
; interpreted as a substitution and replaced with the hexadecimal value of the
; corresponding WORD in the varargs val... (e.g stack).
; @param (WORD) val...: The values to be printed.
DEF_GLOBAL_FUNC(printf32):
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
    call    printfSubstitute32
    add     esp, 0x4
    add     ebx, 0x4
    jmp     .charLoopTail

.charLoopRegular:
    mov     al, [esi]
    push    eax
    call    _putChar32
    add     esp, 0x4

.charLoopTail:
    inc     esi
    jmp     .charLoop

.out:
    pop     ebx
    pop     esi
    leave
    ret


; ##############################################################################
;                               64-bit functions
; ##############################################################################

BITS    64

; ==============================================================================
; Scroll the VGA buffer up one line.
scrollVgaBufferUp64:
    mov     rcx, VGA_BUFFER_ROWS * (VGA_BUFFER_COLS - 1)
    mov     rdi, VGA_BUFFER_PHY_ADDR
    mov     rsi, VGA_BUFFER_PHY_ADDR + VGA_BUFFER_COLS * 2
    rep movsw
    ret

; ==============================================================================
; Print a character in the VGA buffer at the current cursor position.
; @param %dil: The char to print.
_putCharVga64:
    push    rbp
    mov     rbp, rsp

    cmp     dil, `\n`
    je      .handleNewLine
    jmp     .handleRegularChar

.handleNewLine:
    ; For new line we simply have to update the index of cursor to start to the
    ; next line, e.g. the next multiple of VGA_BUFFER_COLS*2. The *2 here is
    ; because the VGA buffer is an array of WORDs.
    mov     ax, [cursorPos]
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
    add     [cursorPos], ax
    jmp     .checkIfScrollUpNeededAndOut

.handleRegularChar:
    ; For regular characters we simply print on the current cursorPos and
    ; increment the cursorPos.
    mov     ah, (VGA_BACKGROUND_COLOR << 4) | VGA_FOREGROUND_COLOR
    mov     al, dil
    movzx   esi, WORD [cursorPos]
    mov     [esi + VGA_BUFFER_PHY_ADDR], ax
    ; Advance the cursor to the next char pos in the buffer.
    add     WORD [cursorPos], 2
    ; Fall-through to .checkIfScrollUpNeededAndOut.

.checkIfScrollUpNeededAndOut:
    ; AX = Index of buffer end.
    mov     ax, VGA_BUFFER_ROWS * VGA_BUFFER_COLS * 2
    cmp     [cursorPos], ax
    jb      .out
    ; Need to scroll the buffer up.
    call    scrollVgaBufferUp64
    ; Fixup the cursor to start on the previous line (e.g. the last line).
    sub     WORD [cursorPos], VGA_BUFFER_COLS * 2

.out:
    leave
    ret

; ==============================================================================
; Print a character.
; @param (WORD) c: The char to print.
_putChar64:
    jmp     _putCharVga64

; ==============================================================================
; Helper function for printf, print a value in hexadecimal format.
; @parma %rdi: The value to be printed.
printfSubstitute64:
    push    rbp
    mov     rbp, rsp

    push    rbx
    push    r12
    push    r13

    ; RBX = Value to print
    mov     rbx, rdi

    ; First print the "0x" prefix.
    mov     dil, `0`
    call    _putChar64
    mov     dil, `x`
    call    _putChar64

    ; Now print the value in hex format.
    ; R12 = Mask
    ; This mask is used to extract the 4-bit hex digits from the value.
    mov     r12, (0xf << 60)
    ; R13 = Shift
    ; Used to right shift the bits extracted with Mask.
    mov     r13, 60
.loop:
    test    r12, r12
    jz      .out

    ; AL = Next hex digits.
    mov     rax, rbx
    and     rax, r12
    mov     rcx, r13
    shr     rax, cl

    cmp     al, 0xa
    jae     .loopPrintDigitAbove10
    ; This is a num.
    add     al, `0`
    jmp     .loopPrintChar
.loopPrintDigitAbove10:
    sub     al, 0xa
    add     al, `a`
.loopPrintChar:
    mov     dil, al
    call    _putChar64

    ; Update iteration vars.
    mov     cl, 4
    shr     r12, cl
    sub     r13, 4
    jmp     .loop

.out:

    pop     r13
    pop     r12
    pop     rbx
    leave
    ret

; ==============================================================================
; NEVER CALL THIS FUNCTION DIRECTLY, ALWAYS USE `LOG` MACRO.
; Print a formatted string. 64-bit mode version.
; @param %rdi: Pointer to the formatted NUL-terminated string. Each '$'
; character is interpreted as a substitution and replaced with the hexadecimal
; value of the corresponding DWORD in the varargs val... (e.g stack).
; @param (PUSHED ON STACK) val...: The values to be printed.
DEF_GLOBAL_FUNC(printf64):
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12

    ; r12 = Pointer to next char to print.
    mov     r12, rdi

    ; rbx = Ptr to next value to substitute.
    lea     rbx, [rbp + 0x10]

    ; Loop over each char of the string until we processed the whole string. For
    ; each char:
    ;   * If the char is a regular char print it.
    ;   * If this is a substitution sequence, print the next value.
.charLoop:
    ; Loop condition, check for NUL char.
    cmp     BYTE [r12], 0
    je      .out

    ; Check if this is a substitution char.
    cmp     BYTE [r12], `$`
    jne     .charLoopRegular
    ; This is a substitution char, print the value.
    mov     rdi, [rbx]
    call    printfSubstitute64
    ; Advance pointer to next value to substitute.
    add     rbx, 0x8
    jmp     .charLoopTail

.charLoopRegular:
    mov     dil, [r12]
    call    _putChar64

.charLoopTail:
    inc     r12
    jmp     .charLoop

.out:
    pop     r12
    pop     rbx
    leave
    ret
