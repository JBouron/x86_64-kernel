; Some helper functions for the PANIC() macros. Those functions are not supposed
; to be called by any function/macro other than PANIC() hence why there is no
; associated *.inc file.

%include "macros.mac"

; The following functions are used by the PANIC() macro to print the panic
; banner. Calling functions instead of invocing the _LOG macro within PANIC()
; leads to a smaller image as the strings don't get duplicated.
DEF_GLOBAL_FUNC32(_printPanicHeader32):
    _LOG "==================== PANIC ===================="
    ret
DEF_GLOBAL_FUNC32(_printPanicBottom32):
    _LOG "==============================================="
    ret
DEF_GLOBAL_FUNC64(_printPanicHeader64):
    _LOG "==================== PANIC ===================="
    ret
DEF_GLOBAL_FUNC64(_printPanicBottom64):
    _LOG "==============================================="
    ret
