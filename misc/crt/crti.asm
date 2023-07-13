; Start of the .init and .fini sections. GCC will insert calls to global
; constructors and destructors in those sections.
; While global constructors are useful, there is little to no reason to call
; global destructor instead of just rebooting. Yet, for completeness, and also
; because GCC expects it, we add the .fini and _fini here.

SECTION .init

; _init - Call all global constructors.
GLOBAL  _init:function
_init:
    push    rbp
    mov     rbp, rsp
    ; GCC will append the rest of this section/function with calls to ctors.

SECTION .fini

; _fini - Call all global constructors.
GLOBAL  _fini:function
_fini:
    push    rbp
    mov     rbp, rsp
    ; GCC will append the rest of this section/function with calls to dtors.
