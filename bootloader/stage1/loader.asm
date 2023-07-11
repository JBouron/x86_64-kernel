; Utilities to load the kernel from disk.

%include "macros.mac"
%include "disk.inc"
%include "malloc.inc"

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
DEF_LOCAL_FUNC64(loadMetadataSector):
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

; Pointer to a buffer containing the content of the metadata sector.
DEF_LOCAL_VAR(metadataSecBuffer):
DQ  0x0

; Size of the ELF header in a ELF file. For 64-bit ELF files (which we are
; targetting) this is 64 bytes.
ELF_HEADER_SIZE EQU 64

; Offsets of the different fields in a ELF header, at least the fields we are
; interested in.
; Magic number
ELF_HDR_MAG         EQU 0x00    ; DWORD
ELF_HDR_CLASS       EQU 0x04    ; BYTE
ELF_HDR_DATA        EQU 0x05    ; BYTE
ELF_HDR_TYPE        EQU 0x10    ; WORD
ELF_HDR_MACHINE     EQU 0x12    ; WORD
ELF_HDR_ENTRY       EQU 0x18    ; QWORD
ELF_HDR_PHOFF       EQU 0x20    ; QWORD
ELF_HDR_PHENTSIZE   EQU 0x36    ; WORD
ELF_HDR_PHNUM       EQU 0x38    ; WORD

; Magic number value of an ELF file.
ELF_HEADER_MAGIC_VALUE    EQU 0x464c457f


; ==============================================================================
; Load the kernel from disk
; TODO: Implement this.
DEF_GLOBAL_FUNC64(loadKernel):
    push    rbp
    mov     rbp, rsp

    call    loadMetadataSector
    DEBUG   "Metadata sector data @$:", rax
    DEBUG   "  Kernel start sector = $", \
        QWORD [rax + MS_KERNEL_IMAGE_SECTOR_START_OFF]
    DEBUG   "  Kernel bytes        = $", QWORD [rax + MS_KERNEL_IMAGE_BYTES_OFF]

    ; Save buffer containing the metadata sector.
    mov     [metadataSecBuffer], rax

    ; Read the ELF header.
    mov     rdi, [rax + MS_KERNEL_IMAGE_SECTOR_START_OFF]
    shl     rdi, 9
    sub     rsp, ELF_HEADER_SIZE
    mov     rsi, rsp
    call    readElfHeaderFromDisk
    test    rax, rax
    jz      .readElfHeaderOk
    PANIC   "Failed to read ELF header from disk"
.readElfHeaderOk:
    DEBUG   "Loaded ELF header from disk"

    DEBUG   "ELF is an x86_64 executable:"
    mov     rax, [rsp + ELF_HDR_ENTRY]
    DEBUG   "   .e_entry     = $", rax
    mov     rax, [rsp + ELF_HDR_PHOFF]
    DEBUG   "   .e_phoff     = $", rax
    movzx   rax, WORD [rsp + ELF_HDR_PHENTSIZE]
    DEBUG   "   .e_phentsize = $", rax
    movzx   rax, WORD [rsp + ELF_HDR_PHNUM]
    DEBUG   "   .e_phnum     = $", rax

    mov     rdi, rsp
    call    parseProgramHeaderTable

    leave
    ret


; ==============================================================================
; Read data from the kernel image into a buffer.
; @param RDI: Offset to read from in the image, relative to the start of the
; image.
; @param RSI: Number of bytes to read from the image.
; @param RDX: Buffer to read data into.
; @return RAX: If successful returns 0, else return 1.
DEF_LOCAL_FUNC64(readKernelImage):
    push    rbp
    mov     rbp, rsp

    ; First check that the read is within the bounds of the kernel image.
    lea     rax, [rdi + rsi]
    mov     rcx, [metadataSecBuffer]
    mov     rcx, [rcx + MS_KERNEL_IMAGE_BYTES_OFF]
    cmp     rax, rcx
    jbe     .withinBounds
    PANIC   "Attempted to read outside of kernel image file boundaries"
.withinBounds:

    ; Do the read.
    mov     rcx, rdx
    mov     rdx, rsi
    mov     rsi, [metadataSecBuffer]
    mov     rsi, [rsi + MS_KERNEL_IMAGE_SECTOR_START_OFF]
    shl     rsi, 9
    add     rsi, rdi
    movzx   rdi, BYTE [bootDriveIndex]
    DEBUG   "Read from offset $", rsi
    call    readBuffer

    ; FIXME: Handle errors once returned by readBuffer.
    xor     rax, rax

    leave
    ret


; ==============================================================================
; Read the ELF header from disk into a destination buffer. This function also
; makes sure that this ELF describes an x86_64 executable.
; @param %RDI: Start offset of the ELF file on disk.
; @param %RSI: The destination buffer to read the header into. This buffer must
; be at least 64 bytes.
; @return %RAX: On success returns 0, else return 1.
DEF_LOCAL_FUNC64(readElfHeaderFromDisk):
    push    rbp
    mov     rbp, rsp

    ; Save RSI as it points to the buffer in which the header is to be read. We
    ; will parse the header after reading it to make sure to conforms to what we
    ; expect.
    push    rsi

    mov     rdi, 0x0
    mov     rdx, rsi
    mov     rsi, ELF_HEADER_SIZE
    call    readKernelImage
    test    rax, rax
    jz      .readOk
    PANIC   "Could not read ELF header from disk"
.readOk:
    pop     rsi

    ; Check the magic number.
    cmp     DWORD [rsi], ELF_HEADER_MAGIC_VALUE
    je      .elfMagicOk
    mov     eax, [rsi]
    PANIC   "Magic number $ does not match ELF magic number", rsi
.elfMagicOk:

    ; Check that the ELF is conforming to what we expect and are capable of
    ; loading, namely:
    ;   The Class should be 2 indicating 64-bit format.
    ;   The Data should be 1 indicating little-endian.
    ;   The Type should be 0x02, indicating executable file.
    ;   The Machine should be 0x3e, indicating x86_64 target.
    cmp     BYTE [rsi + ELF_HDR_CLASS], 2
    je      .elfClassOk
    movzx   rsi, BYTE [rsi + ELF_HDR_CLASS]
    PANIC   "Unsupported ELF class $, expected 2 (64-bit format)", rsi
.elfClassOk:
    cmp     BYTE [rsi + ELF_HDR_DATA], 1
    je      .elfDataOk
    movzx   rsi, BYTE [rsi + ELF_HDR_DATA]
    PANIC   "Unsupported ELF data $, expected 1 (little-endian)", rsi
.elfDataOk:
    cmp     WORD [rsi + ELF_HDR_TYPE], 0x02
    je      .elfTypeOk
    movzx   rsi, BYTE [rsi + ELF_HDR_TYPE]
    PANIC   "Unsupported ELF type $, expected 0x02 (executable file)", rsi
.elfTypeOk:
    cmp     WORD [rsi + ELF_HDR_MACHINE], 0x3e
    je      .elfMachineOk
    movzx   rsi, BYTE [rsi + ELF_HDR_MACHINE]
    PANIC   "Unsupported ELF machine $, expected 0x3e (x86_64)", rsi
.elfMachineOk:
    cmp     WORD [rsi + ELF_HDR_PHENTSIZE], 0x38
    je      .elfPhentsizeOk
    movzx   rsi, WORD [rsi + ELF_HDR_PHENTSIZE]
    PANIC   "Uh?? Program header entry size is $, expected 0x38", rsi
.elfPhentsizeOk:

    ; FIXME: Once readBuffer has proper error handling, forward any error to the
    ; caller.
    xor     rax, rax

    leave
    ret


ELF_PHT_TYPE    EQU 0x00    ; DWORD
ELF_PHT_FLAGS   EQU 0x04    ; DWORD
ELF_PHT_OFF     EQU 0x08    ; QWORD
ELF_PHT_VADDR   EQU 0x10    ; QWORD
ELF_PHT_PADDR   EQU 0x18    ; QWORD
ELF_PHT_FILESZ  EQU 0x20    ; QWORD
ELF_PHT_MEMSZ   EQU 0x28    ; QWORD
ELF_PHT_ALIGN   EQU 0x30    ; QWORD


; ==============================================================================
; Call-back called on each program header table entry.
; @param %RDI: Pointer to the program header table entry.
DEF_LOCAL_FUNC64(handleProgramHeaderTableEntry):
    push    rbp
    mov     rbp, rsp

    ; Check type, a type of 0x0 indicates that the entry in un-used hence we can
    ; skip. Otherwise we expect to only see PT_LOAD entries, e.g. loadable
    ; segments.
    mov     eax, [rdi + ELF_PHT_TYPE]
    test    eax, eax
    jz      .out
    cmp     eax, 0x1
    je      .loadableSegment
    PANIC   "Unsupported segment type $", rax
.loadableSegment:

    DEBUG   "Program Header Table Entry:"
    mov     eax, [rdi + ELF_PHT_FLAGS]
    DEBUG   "  .flags  = $  .offset = $", rax, QWORD [rdi + ELF_PHT_OFF]
    DEBUG   "  .vaddr  = $  .paddr  = $", \
        QWORD [rdi + ELF_PHT_VADDR], QWORD [rdi + ELF_PHT_PADDR]
    DEBUG   "  .filesz = $  .memsz  = $", \
        QWORD [rdi + ELF_PHT_FILESZ], QWORD [rdi + ELF_PHT_MEMSZ]
    DEBUG   "  .align  = $", QWORD [rdi + ELF_PHT_ALIGN]

    ; TODO: Load the section/segment to virtual memory and update the page table
    ; accordingly.

.out:
    leave
    ret


; ==============================================================================
; Parse the program header table. Calls handleProgramHeaderTableEntry on each
; entry of the table.
; @param %RDI: Pointer to the ELF header.
DEF_LOCAL_FUNC64(parseProgramHeaderTable):
    push    rbp
    mov     rbp, rsp
    push    r15
    push    r14

    ; R15D = Number of program headers in program header table.
    mov     r15w, [rdi + ELF_HDR_PHNUM]
    DEBUG   "$ program header table entries", r15

    ; R14 = Disk offset of current Program header table entry.
    mov     r14, [rdi + ELF_HDR_PHOFF]
    DEBUG   "r14 = $", r14
    ; For all program header table entries...
.loopCond:
    test    r15w, r15w
    jz      .loopOut
.loopTop:
    ; Read entry on stack.
    ; RSP = Program header table entry.
    mov     rax, 0x38
    sub     rsp, rax

    mov     rdi, r14
    mov     rsi, rax
    mov     rdx, rsp
    call    readKernelImage
    test    rax, rax
    jz      .readOk
    PANIC   "Failed to read program header table entry from kernel image"
.readOk:
    mov     rdi, rsp
    call    handleProgramHeaderTableEntry

.loopNextIte:
    add     rsp, 0x38
    dec     r15w
    add     r14, 0x38
    jmp     .loopCond
.loopOut:

    pop     r14
    pop     r15
    leave
    ret
