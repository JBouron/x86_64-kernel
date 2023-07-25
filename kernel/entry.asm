; Very first entry point to the kernel from the bootloader.

SECTION .text

; Calls all the global constructors.
EXTERN  _init
; C++ entry point of the kernel.
EXTERN  kernelMain
; Calls all the global destructors.
EXTERN  _fini

GLOBAL kernelEntry:function
kernelEntry:
    ; We are live! Now at this point we are still using the stack from the
    ; bootloader, but that is fine for now, eventually we should allocate a
    ; bigger stack somewhere where it does not risk overwriting BIOS data
    ; (FIXME).

    ; The bootloader passed us the pointer to the bootstruct in %RDI. This
    ; pointer will be passed to the main, save it for now.
    push    rdi

    ; First thing to do is to call global contructors. Note that at this point
    ; the environment is pretty bare, logging as not be enabled yet so we can
    ; only do the most basic things in the global constructors.
    call    _init

    ; Call the kernel main with the pointer to the bootstruct as param.
    pop     rdi
    call    kernelMain

    ; In case we ever return from main, just halt forever at this point.
.dead:
    cli
    hlt
    jmp     .dead

