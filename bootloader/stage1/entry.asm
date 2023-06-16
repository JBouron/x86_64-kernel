; Entry point of stage 1. Stage 0 loads this stage and jumps to stage1Entry.

%include "macros.mac"
%include "logger.inc"
%include "pm.inc"
%include "tests/tests.inc"
%include "memmap.inc"
%include "lm.inc"

SECTION .text

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
DEF_GLOBAL_FUNC(stage1Entry):
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
    INFO    "Successfully jumped to 32-bit protected mode"
    mov     ebx, esp
    DEBUG   "esp = $", ebx

    push    stage1Entry64
    call    jumpToLongMode

.dead:
    hlt
    jmp     .dead

; ==============================================================================
; Entry point for stage 1 in 64bit protected-mode. stage1Entry32 jumps to this
; code.
stage1Entry64:
    BITS    64

    INFO    "Successfully jumped to 64-bit long mode"
    mov     rbx, rsp
    DEBUG   "rsp = $", rbx

    INFO    "Running self-tests"
    call    runSelfTests
.dead:
    hlt
    jmp     .dead

BITS    64

; ==============================================================================
; Run stage1 self tests.
; Note on test functions: Test functions are not taking arguments and are
; expected to lock-up the CPU in case of failure (after hopefully printing
; a useful error message). Naturally, test functions are expected to return
; the CPU in a sane/consistent state.
runSelfTests:
    ; All test functions should be called from here.
    RUN_TEST(callBiosFuncTest)

    ; FIXME: 64-bit arithmetic functions are not tested anymore as they will
    ; most likely disappear due to not having a use for them anymore.
    ;RUN_TEST(add64Test)
    ;RUN_TEST(sub64Test)
    ;RUN_TEST(cmp64Test)

    ret
