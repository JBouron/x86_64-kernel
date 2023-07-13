; End of the .init and .fini sections. All this code follows the calls to global
; constructors/destructors.

SECTION .init
    ; End of the _init function declared in crti.asm.
    ; Above this are all the calls to the global ctors.
    leave
    ret

SECTION .fini
    ; End of the _fini function declared in crti.asm.
    ; Above this are all the calls to the global dtors.
    leave
    ret
