; Helper functions to call BIOS functions from 32-bit Protected-Mode.
%ifndef BIOSCALL_INC
%define BIOSCALL_INC

%include "macros.mac"
%include "bioscallconsts.inc"

; Call a BIOS function from 32-bit protected-mode. This function takes care of
; jumping back to real-mode, execute the BIOS function, re-enable 32-bit
; protected-mode and return the result of the call.
; Caller to this function are passing a pointer to a BIOSCallPacket structure
; which is used to both pass the arguments for the BIOS call and the results
; (arguments and results here being the values of the GP registers before and
; after the call respectively). The constants above this command indicate the
; offset and size of each field in a BIOSCallPacket (BCP) structure.
; From the point of view of the caller, this is a regular function call, the
; fact that this function goes to real-mode and back to 32-bit protected-mode is
; invisible to the caller.
; @param (DWORD) bcpAddr: Pointer to a BCP packet. This address must be <65k
; since it needs to be accessed from real-mode. The struct is modified in-place,
; indicating the value of each register after the BIOS function returned.
EXT_FUNC(callBiosFunc)

%endif
; vim:syntax=asm filetype=asm
