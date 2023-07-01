; Disk access routines.
%ifndef DISK_INC
%define DISK_INC

%include "macros.mac"

; ==============================================================================
; Read sectors from disk.
; @param (BYTE) %RDI: The index of the disk to read from.
; @param %RSI: The LBA index of the first sector to read.
; @param (WORD) %RDX: The number of sectors to read.
; @param %RCX: Destination address where to write the sectors in memory.
; @return %RAX: The number of sectors successfully read and written to memory.
; This function stops and returns at the first error encountered.
EXT_FUNC(readDiskSectors)

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
EXT_FUNC(readBuffer)

%endif
; vim:syntax=asm filetype=asm
