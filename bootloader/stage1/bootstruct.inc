; Bootstruct related functions, callers of these functions should not care of
; the actual content and layout of the bootstruct, hence only exporting the
; minimum needed to create a bootstruct.
; See bootstruct.asm for more details on the bootstruct.
%ifndef BOOTSTRUCT_INC
%define BOOTSTRUCT_INC

%include "macros.mac"

; ==============================================================================
; Allocate a bootstruct on the heap and initializes it's content.
; @return %RAX: The address of the allocated bootstruct.
EXT_FUNC(createBootStruct)


%endif
; vim:syntax=asm filetype=asm
