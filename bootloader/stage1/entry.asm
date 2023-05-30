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

BITS    16

; ==============================================================================
; Entry point for stage 1. At this point the following guarantees hold:
;   * Interrupts are disabled.
;   * A20 line is enabled.
;   * All segment registers are 0x0000.
;   * A stack has been set up for us.
;   * We are still in real mode.
stage1Entry:
    mov     ax, 0xdead
    mov     bx, 0xbeef

.dead:
    hlt
    jmp     .dead

; Pad with zeros until next sector boundary.
ALIGN   512, DB 0
stage1End:
