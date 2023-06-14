%include "tests/tests.inc"
%include "arith64.inc"

BITS    32

; Generate a test case for 64-bit arithmetic. The generated code calls the given
; function (%1) with the given operands (%3 and %4) and checks the result of the
; function against the expected value computed as "%3 %2 %4" where %2 is the
; operator that is implemented by the function. If the result is not as expected
; the generated code calls the TEST_FAIL macro.
; @param %1: The function implementing the operation, e.g. add64, sub64, ...
; @param %2: The operator, e.g. +, -, ... This is used to generate the expected
; value.
; @param %3: The first operand.
; @param %4: The second operand.
%macro ARITH64_TEST_CASE 4
    push    ebx
    ; Dest
    sub     esp, 8
    mov     edx, esp
    ; A
    push    (%3 >> 32)
    push    (%3 & 0xffffffff)
    mov     eax, esp
    ; B
    push    (%4 >> 32)
    push    (%4 & 0xffffffff)
    mov     ebx, esp

    push    ebx
    push    eax
    push    edx
    call    %1
    add     esp, 28

    cmp     DWORD [esp], ((%3 %2 %4) & 0xffffffff)
    jne     %%fail
    cmp     DWORD [esp + 4], ((%3 %2 %4) >> 32)
    jne     %%fail
    jmp     %%ok
    %%fail:
    pop     ebx
    TEST_FAIL   "Invalid result"
    %%ok:
    pop     ebx
%endmacro

; Test case for add64.
; @param %1: The first operand.
; @param %2: The second operand.
%macro ADD64_TEST_CASE 2
    ARITH64_TEST_CASE add64, +, %1, %2
%endmacro

; ==============================================================================
; Test the add64 function.
DEF_GLOBAL_FUNC(add64Test):
    push    ebp
    mov     ebp, esp

    ; Trivial cases.
    ADD64_TEST_CASE 0x0, 0x0
    ADD64_TEST_CASE 0x1, 0x0
    ADD64_TEST_CASE 0x0, 0x1

    ; Carry propagation
    ADD64_TEST_CASE 0x1, 0xffffffff

    ; Full 64-bit values
    ADD64_TEST_CASE 0xdeadbeefcafebabe, 0xcafebeefdeadbabe

    ; Overflow
    ADD64_TEST_CASE 0xffffffffffffffff, 0x1

    TEST_SUCCESS

; Test case for sub64.
; @param %1: The first operand.
; @param %2: The second operand.
%macro SUB64_TEST_CASE 2
    ARITH64_TEST_CASE sub64, -, %1, %2
%endmacro

; ==============================================================================
; Test the sub64 function.
DEF_GLOBAL_FUNC(sub64Test):
    push    ebp
    mov     ebp, esp

    ; Trivial cases.
    SUB64_TEST_CASE 0x0, 0x0
    SUB64_TEST_CASE 0x1, 0x0

    ; Carry propagation
    SUB64_TEST_CASE 0x0, 0x1

    ; Full 64-bit values
    SUB64_TEST_CASE 0xdeadbeefcafebabe, 0xcafebeefdeadbabe

    TEST_SUCCESS

; Generate a test-case for cmp64. If the test case fails, TEST_FAIL is called.
; @param %1: The first operand.
; @param %2: The second operand.
; @param %3: The expected result when calling cmp64 with the first and second
; operand.
%macro CMP64_TEST_CASE 3
    ; A
    push    (%1 >> 32)
    push    (%1 & 0xffffffff)
    mov     eax, esp
    ; B
    push    (%2 >> 32)
    push    (%2 & 0xffffffff)
    mov     ecx, esp

    push    ecx
    push    eax
    call    cmp64
    add     esp, 8

    cmp     eax, %3
    je      %%ok
    TEST_FAIL   "Incorrect comparison result"
    %%ok:
%endmacro

; ==============================================================================
; Test the cmp64 function.
DEF_GLOBAL_FUNC(cmp64Test):
    push    ebp
    mov     ebp, esp

    CMP64_TEST_CASE 0x0, 0x0, 0
    CMP64_TEST_CASE 0x0, 0x1, -1
    CMP64_TEST_CASE 0x1, 0x0, 1
    CMP64_TEST_CASE 0xdeadbeef00000000, 0xcafebabe00000000, 1
    CMP64_TEST_CASE 0x00000000deadbeef, 0x00000000cafebabe, 1
    CMP64_TEST_CASE 0xdeadbeef00000000, 0xdeadbeef11111111, -1
    CMP64_TEST_CASE 0xdeadbeefcafebabe, 0xdeadbeefcafebabe, 0
    CMP64_TEST_CASE 0x0000000000000000, 0xffffffffffffffff, -1
    CMP64_TEST_CASE 0xffffffffffffffff, 0x0000000000000000, 1

    TEST_SUCCESS
