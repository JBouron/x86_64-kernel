; Helper functions to call BIOS functions from 32-bit Protected-Mode.
%ifndef BIOSCALL_INC
%define BIOSCALL_INC

%include "macros.mac"
%include "bioscallconsts.inc"

; ==============================================================================
; Call a BIOS function from 64-bit long-mode. This function takes care of
; jumping all the way back to real-mode, execute the BIOS function, return to
; 64-bit long-mode and return the result of the call. All mode switching is
; transparent to the caller.
; Caller to this function is passing a pointer to a BIOSCallPacket structure
; which is used to both pass the arguments for the BIOS call and the results
; (arguments and results here being the values of the GP registers before and
; after the call respectively). See bioscallconst.inc for the offset and size of
; each field in a BIOSCallPacket (BCP) structure.
; @param %RDI bcpAddr: Pointer to a BCP packet. This address must be <65k since
; it needs to be accessed from real-mode (real-mode uses 0x0000 data segment).
; The struct is modified in-place, indicating the value of each register after
; the BIOS function returned.
EXT_FUNC(callBiosFunc)

%endif
; vim:syntax=asm filetype=asm
