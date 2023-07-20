SECTION .text
BITS    64

EXTERN  interruptTestFlag

GLOBAL  interruptTestHandler0:function
interruptTestHandler0:
    mov     QWORD [interruptTestFlag], 0
    iretq

GLOBAL  interruptTestHandler1:function
interruptTestHandler1:
    mov     QWORD [interruptTestFlag], 1
    iretq

GLOBAL  interruptTestHandler3:function
interruptTestHandler3:
    mov     QWORD [interruptTestFlag], 3
    iretq
