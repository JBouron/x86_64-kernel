; Disk access routines.
%include "macros.mac"
%include "bioscall.inc"
%include "bioscallconsts.inc"

BITS    64

; Size of a disk sector in bytes.
SECTOR_SIZE EQU 512

; Disk Access Packet related constants. The Disk Access Packet (DAP) is a
; small structure used by the INT 13h AH=42h BIOS function to describe which
; sectors should be read from disk and where to write the read sectors in
; memory. It consists of the following fields:
; u8 size: The size of the structure in bytes, must be 0x10.
DAP_SIZE_OFF        EQU 0x0
; u8 unused: Unused byte, must be 0x0.
DAP_UNUSED_OFF      EQU 0x1
; u16 numSectorsToRead: The number of sectors to be read from disk.
DAP_NUMSECTORS_OFF  EQU 0x2
; u32 destBuffer: The destination buffer where to write the sectors data. Note
; that this is a real-mode address of the form segment:offset in little-endian
; (e.g. offset is the low u16 and segment is the high u16).
DAP_DESTBUF_OFF     EQU 0x4
; u64 firstSectorLBA: The start LBA index of the sectors to be read.
DAP_STARTLBA_OFF    EQU 0x8

; Size of the DAP structure.
DAP_SIZE            EQU 0x10

; ==============================================================================
; Read sectors from disk.
; @param (BYTE) %RDI: The index of the disk to read from.
; @param %RSI: The LBA index of the first sector to read.
; @param (WORD) %RDX: The number of sectors to read.
; @param %RCX: Destination address where to write the sectors in memory.
; @return %RAX: The number of sectors successfully read and written to memory.
; This function stops and returns at the first error encountered.
DEF_GLOBAL_FUNC64(readDiskSectors):
    push    rbp
    mov     rbp, rsp

    DEBUG   "Read from disk $:", rdi
    DEBUG   "  First sector  = $", rsi
    DEBUG   "  Num sector(s) = $", rdx
    DEBUG   "  Dest buffer   = $", rcx

    ; In order to read sectors, we use the BIOS function INT 13h AH=42h:
    ; Extended Read Sectors From Drive
    ; The drawback of this function is that the destination memory buffer where
    ; the sectors should be written-to is a real-mode address, hence we can only
    ; write to offsets <1MiB. Therefore, if the destination address is >=1MiB we
    ; need to first write in a temporary location under 1MiB and then copy the
    ; data to the "high" address. To keep things simple we do exactly that in
    ; all cases, even if the dest address is <1MiB.

    push    r15
    push    r14
    push    r13
    push    r12

    ; Save number of sectors to read, used for return value.
    push    rdx

    ; R15 = Number of sectors left to read / loop iteration counter.
    movzx   r15, dx
    ; R14 = Write pointer.
    mov     r14, rcx
    ; R13 = Next sector's LBA index to be read.
    mov     r13, rsi
    ; R12b = Drive index.
    mov     r12b, dil

    ; Read all sectors one-by-one. After reading a sector, immediately copy it
    ; to the destination memory address. There is potential for optimization
    ; here: we could read and copy multiple sectors at once, reducing the number
    ; of iteration and overhead. We keep things simple for now.
    ; All read operations are writing sector data onto the stack before being
    ; copied to their final destination.

    ; Avoid overhead and re-use most of the data structures needed for the read
    ; operations between iterations. Namely we need:
    ;   * A BCP
    ;   * A 16 bytes Disk Address Packet (DAP), as expected by this particular
    ;   read function.
    ;   * A space to store sector data (as mentioned above this is on the stack)
    sub     rsp, SECTOR_SIZE + BCP_SIZE + DAP_SIZE
    ; Make sure the DAP is fully accessible from real-mode when using a DS of
    ; 0x0000.
    lea     rax, [rsp + BCP_SIZE + 0xf]
    test    rax, ~0xffff
    jz      .loopCond
    PANIC   "DAP address cannot be accessed from real-mode DS = 0x0000"
.loopCond:
    test    r15, r15
    jz      .loopOut
.loopTop:
    ; Fill in the BCP and Disk Address Packet.
    ; RSP points to BCP, RSP + BCP_SIZE points to the DAP and RSP + BCP_SIZE +
    ; DAP_SIZE points to where to write sector data.
    ; Technically the function should not clobber registers it is not using as
    ; output but never trust a BIOS function and always set the register values
    ; in the BCP between iterations.
    mov     BYTE [rsp + BCP_IN_INT_OFF], 0x13
    mov     BYTE [rsp + BCP_INOUT_AH_OFF], 0x42
    mov     BYTE [rsp + BCP_INOUT_DL_OFF], r12b
    lea     rax, [rsp + BCP_SIZE]
    mov     WORD [rsp + BCP_INOUT_SI_OFF], ax
    ; Fill in DAP. RAX is pointing at the DAP at this point.
    mov     BYTE [rax + DAP_SIZE_OFF], DAP_SIZE
    mov     BYTE [rax + DAP_UNUSED_OFF], 0x0
    mov     WORD [rax + DAP_NUMSECTORS_OFF], 0x1
    ; Construct the destination buffer real-mode seg:off address. Note that
    ; callBiosFunc uses DS = 0x0000.
    lea     ecx, [rsp + BCP_SIZE + DAP_SIZE]
    and     ecx, 0xffff
    mov     DWORD [rax + DAP_DESTBUF_OFF], ecx
    ; QWORD: LBA index to read from.
    mov     [rax + DAP_STARTLBA_OFF], r13

    ; Read the sector
    lea     rdi, [rsp]
    call    callBiosFunc

    ; Check for error.
    mov     al, [rsp + BCP_OUT_JC]
    test    al, al
    jz      .readOk
    ; An error occured reading the sector, break the loop and return.
    movzx   rax, BYTE [rsp + BCP_INOUT_AH_OFF]
    CRIT    "Error while reading sector. Err code = $", rax
    jmp     .loopOut
.readOk:
    ; Read was successful, copy the sector data from the stack to the memory
    ; location.
    lea     rsi, [rsp + BCP_SIZE + DAP_SIZE]
    mov     rdi, r14
    mov     rcx, SECTOR_SIZE / 8
    cld
    rep movsq
    ; Fall-through next iteration.
.loopNextIte:
    ; Increment read LBA address.
    inc     r13
    ; Advance write pointer.
    add     r14, SECTOR_SIZE
    ; One less sector to be read.
    dec     r15
    jmp     .loopCond
.loopOut:
    ; We end-up no matter if an error occured of not.
    ; De-allocate all the stuff from the stack.
    add     rsp, SECTOR_SIZE + BCP_SIZE + DAP_SIZE
    ; Compute how many sectors were successfully read for the return value.
    ; Number of sectors to read was saved on stack
    pop     rax
    ; Sub number of sectors left to read to get the number read so far.
    sub     rax, r15

    pop     r12
    pop     r13
    pop     r14
    pop     r15
    leave
    ret


; ==============================================================================
; Read an arbitrary number of bytes from an arbitrary offset from disk. Neither
; the number of bytes to be read nor the offset to read from have have to be
; multiples of sector size. This function takes care of figuring out which
; sectors need to be read to satisfy the read operation.
; This function does not return and insteads PANICs if an error occurs FIXME.
; @param (BYTE) %RDI: Index of the drive to read from.
; @param %RSI: Offset to read from on disk. Byte granularity.
; @param %RDX: Number of bytes to read from disk.
; @param %RCX: Destination buffer to read into. Must be of size >= %RSI.
DEF_GLOBAL_FUNC64(readBuffer):
    push    rbp
    mov     rbp, rsp

    push    r15
    push    r14
    push    r13
    push    r12

    ; R15 = Number of bytes left to read.
    mov     r15, rdx
    ; R13 = read pointer, e.g. offset to read from disk. Updated at every
    ; iteration.
    mov     r13, rsi
    ; R12B = disk id.
    movzx   r12, dil
    ; RBX = write pointer in the destination buffer. Updated at every iteration.
    mov     rbx, rcx

    ; While number of bytes left to read > 0.
.loopCond:
    test    r15, r15
    jz      .loopOut
.loopTop:
    ; Check if we can read the sector data directly into the destination buffer,
    ; e.g. without any additional copy. This is only possible if the following
    ; holds:
    ;   1. The read pointer is on a sector boundary.
    ;   2. There are at least 512 bytes left to read.
    ; In case any of the above is false, we need to first read the sector into a
    ; temporary buffer and then copy the data of interest to the destination
    ; buffer.
    test    r13, ((1 << 9) - 1)
    jnz     .slowPath
    cmp     r15, 512
    jb      .slowPath

    ; We are in the "fast case" where the sector is read directly into the
    ; destination buffer without additional copy.
    ; FIXME: Instead of always reading a single sector we can read multiple
    ; sectors at a time if there is enough bytes left to read.
    mov     rdi, r12
    mov     rsi, r13
    shr     rsi, 9
    mov     rdx, 1
    mov     rcx, rbx
    call    readDiskSectors
    cmp     rax, 1
    jne     .fail

    ; Update loop vars and go to next iteration.
    mov     rax, 512
    ; Advance read and write pointers.
    add     r13, rax
    add     rbx, rax
    ; Account for the read bytes.
    sub     r15, rax
    jmp     .loopNextIte

.slowPath:
    ; We cannot read the sector directly into the buffer, either because the
    ; read offset is not aligned on a sector or because there is less than 512
    ; bytes left to read (or both!).
    ; Read the sector into a temporary buffer and then copy the data of interest
    ; into the destination buffer.

    ; Sector is read on stack.
    sub     rsp, 512

    ; Read the sector and store it on the stack.
    mov     rdi, r12
    mov     rsi, r13
    shr     rsi, 9
    mov     rdx, 1
    mov     rcx, rsp
    call    readDiskSectors
    cmp     rax, 1
    jne     .fail

    ; Copy into destination buffer.
    ; The offset within the sector at which the copy starts is:
    ;   readPtr % 512
    ; Hence source addr = RSP + (readPtr % 512)
    ;                   = RSP + (readPtr & (512 - 1))
    ; RAX = readPtr % 512
    mov     rax, r13
    and     rax, ((1 << 9) - 1)
    lea     rsi, [rsp + rax]
    mov     rdi, rbx
    ; Number of bytes to copy = min(512 - RAX, R15)
    mov     rcx, 512
    sub     rcx, rax
    cmp     rcx, r15
    cmova   rcx, r15
    ; Do the copy using REP MOVSB. Save the RCX because it contains the number
    ; of bytes that were copied/read which will be used to update the iteration
    ; variables.
    push    rcx
    cld
    rep movsb
    pop     rcx

    ; De-alloc buffer on stack.
    add     rsp, 512

    ; Update iteration vars.
    add     r13, rcx
    add     rbx, rcx
    sub     r15, rcx
    jmp     .loopNextIte

.loopNextIte:
    jmp     .loopCond
.loopOut:

    pop     r12
    pop     r13
    pop     r14
    pop     r15
    leave
    ret
.fail:
    PANIC   "Failed to read sector"
