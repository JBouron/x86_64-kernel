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

%endif
; vim:syntax=asm filetype=asm
