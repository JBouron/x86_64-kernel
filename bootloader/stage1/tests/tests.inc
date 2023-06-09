; Helper macros, function, ... for running tests.
%ifndef TESTS_INC
%define TESTS_INC

%include "macros.mac"

; Run a test. `name` is the name of the function implementing the test that
; should be called. A test function takes no argument and is expected to return
; EAX=1 if the test is successful and EAX=0 if the test is failed.
%define RUN_TEST(name) _RUN_TEST name
%macro _RUN_TEST 1
    EXTERN  %1
    call    %1
    test    eax, eax
    jnz     %%success
    LOG     %strcat("[FAIL] ", %1)
    jmp     %%out
    %%success:
    LOG     %strcat("[ OK ] ", %1)
    %%out:
%endmacro

; Fail a test case after printing an error message. This essentially logs the
; error message and exit the current function with EAX = 0.
; @param %1: A string litteral containing the error message
%macro TEST_FAIL 1
    LOG     %1
    xor     eax, eax
    leave
    ret
%endmacro

; Ends the test execution, reporting it as successful. This essentially returns
; from the current function call with EAX = 1.
%macro TEST_SUCCESS 0
    mov     eax, 1
    leave
    ret
%endmacro

%endif
; vim:syntax=asm filetype=asm
