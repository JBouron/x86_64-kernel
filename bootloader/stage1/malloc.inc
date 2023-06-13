; Memory allocation routines.
; Memory allocations in stage 1 are never meant to be freed the kernel gets full
; control of the memory once its gets control of the CPU and will mostlikely
; overwrite the memory that was used by the bootloader. This is why there is
; only a malloc() function but no free(). This also makes the design of the
; memory allocator extremly simple: We only keep a pointer at the top of the
; heap and update it after every allocation. No need to keep and maintain a free
; list, ...
%ifndef MALLOC_INC
%define MALLOC_INC

%include "macros.mac"

; Allocate memory.
; @param (DWORD) size: The number of bytes to allocate.
; @return (EAX): The address of the freshly allocated memory. This memory is
; zeroed upon allocation.
EXT_FUNC(malloc)

%endif
; vim:syntax=asm filetype=asm
