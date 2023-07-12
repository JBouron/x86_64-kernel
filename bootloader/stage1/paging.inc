; Paging related functions.
%ifndef PAGING_INC
%define PAGING_INC

%include "macros.mac"

; ==============================================================================
; Map a virtual frame to a physical page frame.
; FIXME: Add parameter for access bits.
; @param %RDI: Virtual address of the page to be mapped. Must be page aligned.
; @param %RSI: Physical address to map the page to. Must be page aligned.
EXT_FUNC(mapPage)

; ==============================================================================
; Allocate virtual memory mapping to newly allocated physical memory.
; @param %RDI: Start of the virtual memory range to allocate. Must be
; page-aligned.
; @parma %RSI: Number of virtual pages to allocate.
EXT_FUNC(allocVirtMem)

%endif
; vim:syntax=asm filetype=asm
