BITS    64
SECTION .text
GLOBAL  lgdt:function
lgdt:
    push    rbp
    mov     rbp, rsp

    lgdt    [rdi]

    leave
    ret
