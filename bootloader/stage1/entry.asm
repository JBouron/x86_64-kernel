; Entry point of stage 1. Stage 0 loads this stage and jumps to stage1Entry.

%include "macros.mac"
%include "logger.inc"
%include "pm.inc"
%include "tests/tests.inc"
%include "memmap.inc"
%include "lm.inc"
%include "loader.inc"

; The index of the drive we have booted from, this information is coming from
; stage 0 in the DL register when jumping to stage1Entry.
DEF_GLOBAL_VAR(bootDriveIndex):
DB  0x0

; ==============================================================================
; Entry point for stage 1. At this point the following guarantees hold:
;   * Interrupts are disabled.
;   * A20 line is enabled.
;   * All segment registers are 0x0000.
;   * A stack has been set up for us.
;   * We are still in real mode.
DEF_GLOBAL_FUNC16(stage1Entry):
    ; Save the boot drive index.
    mov     [bootDriveIndex], dl
    ; Jump to 32-bit protected mode, we don't have anything else to do in
    ; real-mode for now.
    push    stage1Entry32
    call    jumpToProtectedMode
    ; UNREACHABLE
    int3

; ==============================================================================
; Entry point for stage 1 in 32bit protected-mode. stage1Entry jumps to this
; code.
DEF_LOCAL_FUNC32(stage1Entry32):
    ; Since we arrived here from a call to jumpToProtectedMode, the stack still
    ; contains the WORD parameter for jumpToProtectedMode as well as the return
    ; address due to the call. Remove both of them.
    add     esp, 0x4

    call    clearVgaBuffer
    movzx   edx, BYTE [bootDriveIndex]
    INFO    "Successfully jumped to 32-bit protected mode, boot drive = $", edx
    mov     ebx, esp
    DEBUG   "esp = $", ebx

    push    stage1Entry64
    call    jumpToLongMode
    ; UNREACHABLE
    int3

; ==============================================================================
; Entry point for stage 1 in 64bit protected-mode. stage1Entry32 jumps to this
; code.
DEF_LOCAL_FUNC64(stage1Entry64):
    INFO    "Successfully jumped to 64-bit long mode"
    mov     rbx, rsp
    DEBUG   "rsp = $", rbx

    INFO    "Running self-tests"
    call    runSelfTests

    INFO    "Memory map:"
    call    parseMemoryMap

    INFO    "Loading kernel from memory"
    call    loadKernel
.dead:
    hlt
    jmp     .dead

; ==============================================================================
; Run stage1 self tests.
; Note on test functions: Test functions are not taking arguments and are
; expected to lock-up the CPU in case of failure (after hopefully printing
; a useful error message). Naturally, test functions are expected to return
; the CPU in a sane/consistent state.
DEF_LOCAL_FUNC64(runSelfTests):
    ; All test functions should be called from here.
    RUN_TEST(callBiosFuncTest)
    RUN_TEST(memmapFindNextBoundaryTest)
    RUN_TEST(memmapTypeFromBitmapTest)
    RUN_TEST(memmapSanitizeMemMapTest)
    RUN_TEST(readDiskSectorsTest)
    RUN_TEST(readBufferTest)
    RUN_TEST(findFirstAvailFrameTest)
    RUN_TEST(mapPageTest)
    ret
