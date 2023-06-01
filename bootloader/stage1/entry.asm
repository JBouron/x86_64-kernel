; Entry point of stage 1. Stage 0 loads this stage and jumps to stage1Entry.

stage1Start:
; Header used by stage 0 to properly load stage 1 from disk.
; Magic number as expected from stage 0.
DW  0x50f3
; Length of stage 1 in units of disk sectors.
stage1Len   EQU (stage1End - stage1Start)
DW  stage1Len / 512 + (!!(stage1Len % 512))
; Offset of stage 1's entry point relative to the first sector of stage 1.
DW  (stage1Entry - stage1Start)

; Stage 0 always load stage 1 starting at physical address 0x7e00.
ORG 0x7e00

%include "stage1/logger.asm"
%include "stage1/pm.asm"
%include "stage1/bioscall.asm"

; Set the BITS _after_ including other files so that we don't accidentally use
; the BITS set by an included file.
BITS    16

; ==============================================================================
; Entry point for stage 1. At this point the following guarantees hold:
;   * Interrupts are disabled.
;   * A20 line is enabled.
;   * All segment registers are 0x0000.
;   * A stack has been set up for us.
;   * We are still in real mode.
stage1Entry:
    ; Immediately jump to 32-bit protected mode, we don't have anything else to
    ; do in real-mode for now.
    push    stage1Entry32
    call    jumpToProtectedMode
    ; UNREACHABLE
    int3

BITS    32

; ==============================================================================
; Entry point for stage 1 in 32bit protected-mode. stage1Entry jumps to this
; code.
stage1Entry32:
    ; Since we arrived here from a call to jumpToProtectedMode, the stack still
    ; contains the WORD parameter for jumpToProtectedMode as well as the return
    ; address due to the call. Remove both of them.
    add     esp, 0x4

    call    clearVgaBuffer
    mov     ebx, esp
    LOG     "Successfully jumped to 32-bit protected mode, esp = $", ebx

    LOG     "Running self-tests"
    call    runSelfTests
    LOG     "All self-tests passed"

.dead:
    hlt
    jmp     .dead

%include "stage1/tests/bioscallTests.asm"

; ==============================================================================
; Run stage1 self tests.
; Note on test functions: Test functions are not taking arguments and are
; expected to lock-up the CPU in case of failure (after hopefully printing
; a useful error message). Naturally, test functions are expected to return
; the CPU in a sane/consistent state.
runSelfTests:
    push    ebp
    mov     ebp, esp

    ; All test functions should be called from here.
    call    callBiosFuncTest

    leave
    ret

; Pad with zeros until next sector boundary.
ALIGN   512, DB 0
stage1End:
