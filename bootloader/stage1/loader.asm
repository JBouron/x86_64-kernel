; Utilities to load the kernel from disk.

%include "macros.mac"
%include "disk.inc"
%include "malloc.inc"

SECTION .text
BITS    64

; The Metadata Sector
; ===================
; The stage 1 needs information on the kernel image stored on disk before being
; able to load it and execute it. Such information includes the location of the
; image on disk as well as its size.
; To keep things simple, such information is encoded in a special disk sector
; called the Metadata Sector. This sector immediately follows the last stage 1
; sector on disk so that stage 1 can reliably find it and load it.
; The Metadata Sector has a signature/magic-number in its first 8 bytes, this is
; to make sure we are not loading garbage, see METADATA_SECTOR_SIGNATURE.
METADATA_SECTOR_SIGNATURE           EQU 0x617461646174656d
; The rest of the Metadata Sector is organized as follows:
;   - QWORD KERNEL_IMAGE_SECTOR_START: The LBA index of the first sector
;   containing the kernel image. The kernel image is assumed to be stored on
;   contiguous sectors.
MS_KERNEL_IMAGE_SECTOR_START_OFF    EQU 0x08
;   - QWORD KERNEL_IMAGE_BYTES: The size of the kernel image in bytes.
MS_KERNEL_IMAGE_BYTES_OFF           EQU 0x10

; Size of the data in the Metadata Sector, other bytes in the sector are
; irrelevant. For now only the signature and the two QWORDS are stored there.
METADATA_SECTOR_BYTES               EQU 3 * 8

; ==============================================================================
; Load Metadata Sector from disk.
; @return %RAX: A buffer containing the contents of the metadata sector.
loadMetadataSector:
    push    rbp
    mov     rbp, rsp

    ; The disk sector will be read into the stack, the data will then be copied
    ; to an appropriately-sized buffer.
    sub     rsp, 512

    ; Load the Metadata Sector.
    EXTERN  bootDriveIndex
    movzx   rdi, BYTE [bootDriveIndex]
    ; The Metadata Sector LBA index is given by:
    ;   1 + STAGE_1_NUM_SECTORS
    ; the +1 being due to the first sector on disk, which is stage0.
    EXTERN  STAGE_1_NUM_SECTORS
    mov     rsi, STAGE_1_NUM_SECTORS + 1
    mov     rdx, 1
    lea     rcx, [rsp]
    call    readDiskSectors
    test    rax, rax
    jnz     .sectorReadOk
    PANIC   "Failed to read Metadata Sector"
.sectorReadOk:
    ; Verify the signature.
    ; Note: Don't fall into the trap of `cmp xxx, imm64`, this is not a valid
    ; instruction and NASM will truncate the immediate value to 32-bit, which is
    ; sign-extended to 64-bit by the hardware.
    mov     rax, METADATA_SECTOR_SIGNATURE
    cmp     QWORD [rsp], rax
    je      .signatureOk
    mov     rax, [rsp]
    PANIC   "Invalid signature in Metadata Sector: $", rax
.signatureOk:
    ; Metadata Sector seems correct, copy its data (including signature) in a
    ; heap-allocated buffer.
    ; RAX = buffer
    mov     rdi, METADATA_SECTOR_BYTES
    call    malloc
    ; Make the copy.
    mov     rdi, rax
    lea     rsi, [rsp]
    mov     rcx, METADATA_SECTOR_BYTES
    cld
    rep movsb

    ; De-alloc sector from stack.
    add     rsp, 512

    ; RAX still contain the pointer to the buffer at this point. Return.
    leave
    ret

; ==============================================================================
; Load the kernel from disk
; TODO: Implement this.
DEF_GLOBAL_FUNC(loadKernel):
    push    rbp
    mov     rbp, rsp

    call    loadMetadataSector
    DEBUG   "Metadata sector data @$:", rax
    DEBUG   "  Kernel start sector = $", \
        QWORD [rax + MS_KERNEL_IMAGE_SECTOR_START_OFF]
    DEBUG   "  Kernel bytes        = $", QWORD [rax + MS_KERNEL_IMAGE_BYTES_OFF]

    leave
    ret
