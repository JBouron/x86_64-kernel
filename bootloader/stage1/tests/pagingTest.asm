; Test for paging functions.

%include "tests/tests.inc"
%include "paging.inc"

ONE_MIB     EQU (1 << 20)
PAGE_SIZE   EQU 0x1000

; ==============================================================================
; Test the mapPage function.
DEF_GLOBAL_FUNC64(mapPageTest):
    push    rbp
    mov     rbp, rsp
    push    r15

    ; The testing for this function is rather substantial: for each physical
    ; frame in the [0;1MiB[ region, map the frame to vaddr = paddr + 1MiB and
    ; compare the content starting at vaddr with the content starting at paddr
    ; over 4KiB.

    ; R15 = Current page frame paddr.
    xor     r15, r15

    ; For each physical frame at offset <= 1MiB ...
.loopCond:
    cmp     r15, ONE_MIB
    jae     .loopOut
.loopTop:
    ; Map the frame @r15 to vaddr @r15 + 1MiB.
    lea     rdi, [r15 + ONE_MIB]
    mov     rsi, r15
    call    mapPage

    ; Now compare the content of @r15 and @r15 + 1MiB for 4KiB.
    lea     rdi, [r15 + ONE_MIB]
    mov     rsi, r15
    mov     rdx, PAGE_SIZE
    ; FIXME: memcmp should be declared in actual stage1 code, not test code.
    EXTERN  memcmp
    call    memcmp
    test    rax, rax
    jnz     .loopNextIte
    ; The comparison found that the two regions are not the same.
    pop     r15
    TEST_FAIL   "Did not read expected data on mapped virtual address"
.loopNextIte:
    add     r15, PAGE_SIZE
    jmp     .loopCond
.loopOut:

    pop     r15
    TEST_SUCCESS

SECTION .data
ALIGN   PAGE_SIZE
mapPageWriteTestFlag:
DQ  0x0

; ==============================================================================
; Add a mapping to a page an attempt to write into this new mapping. Tests that
; mapPage sets write permission on the mapping.
DEF_GLOBAL_FUNC64(mapPageWriteTest):
    push    rbp
    mov     rbp, rsp

    lea     rax, [mapPageWriteTestFlag]
    DEBUG   "Flag @$", rax

    ; Reset the flag.
    xor     rax, rax
    mov     [mapPageWriteTestFlag], rax

    ; Map the page containing mapPageWriteTestFlag to address 1MiB.
    mov     rdi, ONE_MIB
    lea     rsi, [mapPageWriteTestFlag]
    call    mapPage

    mov     rax, 0xdeadbeefcafebabe

    ; Write to the flag using the new mapping.
    mov     [ONE_MIB], rax

    ; Check that the flag has been written using the ID mapped address.
    TEST_ASSERT_EQ  rax, QWORD[mapPageWriteTestFlag]

    leave
    ret
