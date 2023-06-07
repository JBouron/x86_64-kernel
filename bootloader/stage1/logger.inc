; Utility routines to log messages from stage 1.
%ifndef LOGGER_INC
%define LOGGER_INC

%include "macros.mac"

; Clear the VGA buffer.
EXT_FUNC(clearVgaBuffer)

; Print a formatted string.
; @param (DWORD) str: The formatted NUL-terminated string. Each '$' character is
; interpreted as a substitution and replaced with the hexadecimal value of the
; corresponding DWORD in the varargs val... (e.g stack).
; @param (DWORD) val...: The values to be printed.
EXT_FUNC(printf):

%endif
; vim:syntax=asm filetype=asm
