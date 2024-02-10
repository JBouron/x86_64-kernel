; Assembly routines for the Atomic<T> class.

SECTION .text
BITS    64

; extern "C" u64 _fetchAdd(u64 volatile* const ptr, u64 const add);
GLOBAL  _fetchAdd:function
_fetchAdd:
    lock xadd   [rdi], rsi
    mov     rax, rsi
    ret

; extern bool _cmpxchg(u64* const ptr, u64 const exp, u64 const desired);
GLOBAL  _cmpxchg:function
_cmpxchg:
    mov     rax, rsi
    lock cmpxchg    [rdi], rdx
    setz    al
    ret
