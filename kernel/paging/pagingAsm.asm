; A few assembly code to deal with initializing paging and the direct map.

SECTION .text
BITS    64

PAGE_SIZE EQU 0x1000

EXTERN  directMapMaxMappedOffset
EXTERN  allocFrameFromAssembly

; Initialize the direct map. Implemented in assembly because me thinks this is
; actually faster than C++.
; @param directMapStartAddr: Where to start the direct map in the virtual
; address space. This is essentially the vaddr corresponding to paddr 0x0.
; @param maxPhyAddr: The maximum physical address/offset to map in the direct
; map.
; extern "C" void initializeDirectMap(u64 const directMapStartAddr,
;                                     u64 const maxPhyAddr);
GLOBAL  initializeDirectMap:function
initializeDirectMap:
    push    rbp
    mov     rbp, rsp

    push    r15
    push    r14
    push    r13
    push    r12
    push    rbx

    ; Helper macro to call allocFrameFromAssembly while saving caller-saved
    ; regs.
%macro ALLOC_FRAME 0
    push    r8
    push    r9
    push    r10
    push    r11
    push    rdi
    push    rsi
    call    allocFrameFromAssembly
    pop     rsi
    pop     rdi
    pop     r11
    pop     r10
    pop     r9
    pop     r8
%endmacro

    ; Behold the four nested loops. Yeah, its nasty, but its blazingly fucking
    ; fast. Forget what you were told in class, you don't need functions, you
    ; don't even need C, just assembly, stay with me in wonderland and I will
    ; show you how deep the rabbit hole goes.

    ; What we do here is equivalent to the following pseudo-code:
    ;   curr_offset = 0
    ;   level_4_table = cr3 & ~(PAGE_SIZE - 1)
    ;   level_3_table = 0
    ;   level_2_table = 0
    ;   level_1_table = 0
    ;   for i4 in [i4start, 1, ..., 1 << 9]:
    ;       if level_4_table[i4] is not present:
    ;           allocate new table and make level_4_table[i4] point to it
    ;       level_3_table = level_4_table[i4]
    ;       for i3 in [0, 1, ..., 1 << 9]:
    ;           if level_3_table[i3] is not present:
    ;               allocate new table and make level_3_table[i3] point to it
    ;           level_2_table = level_3_table[i3]
    ;           for i2 in [0, 1, ..., 1 << 9]:
    ;               if level_2_table[i2] is not present:
    ;                   alloc new table and make level_2_table[i2] point to it
    ;               level_1_table = level_2_table[i2]
    ;               for i1 in [0, 1, ..., 1 << 9]:
    ;                   set level_1_table[i1] to point to curr_offset
    ;                   curr_offset += PAGE_SIZE
    ;                   if curr_offset > max_offset:
    ;                       break out of all loops
    ;
    ; i4start is defined as (directMapStartAddr >> (12 + 3 * 9)) & 0x1ff, that
    ; is the index in the PML4 for the directMapStartAddr.
    ; 
    ; The code uses the following mappings:
    ;   curr_offset = RBX
    ;   max_offset = RSI
    ;   level_4_table = R14
    ;   level_3_table = R12
    ;   level_2_table = R10
    ;   level_1_table = R8
    ;   i4 = R15
    ;   i3 = R13
    ;   i2 = R11
    ;   i1 = R9
    ; We try to limit memory accesses as much as possible here, using registers
    ; whenever possible.
    mov     r15, rdi
    shr     r15, 12 + 9 * 3
    and     r15, 0x1ff
    xor     r13, r13
    xor     r11, r11
    xor     r9, r9
    xor     rbx, rbx
    mov     r14, cr3
    and     r14, ~(PAGE_SIZE - 1)
    ; Use RDI as the const: ~directMapStartAddr.
    not     rdi

.level4LoopTop:
    cmp     r15, 0x200
    jae     .level4LoopOut
    mov     rax, [r14 + r15 * 8]
    test    rax, 0x1
    jnz     .level4LoopToNext
    ALLOC_FRAME
    mov     rcx, rax
    or      rcx, 0x3
    and     rcx, rdi
    mov     [r14 + r15 * 8], rcx
.level4LoopToNext:
    and     rax, ~(PAGE_SIZE - 1)
    mov     r12, rax
    xor     r13, r13

    .level3LoopTop:
        cmp     r13, 0x200
        jae     .level3LoopOut
        mov     rax, [r12 + r13 * 8]
        test    rax, 0x1
        jnz     .level3LoopToNext
        ALLOC_FRAME
        mov     rcx, rax
        or      rcx, 0x3
        and     rcx, rdi
        mov     [r12 + r13 * 8], rcx
    .level3LoopToNext:
        and     rax, ~(PAGE_SIZE - 1)
        mov     r10, rax
        xor     r11, r11

        .level2LoopTop:
            cmp     r11, 0x200
            jae     .level2LoopOut
            mov     rax, [r10 + r11 * 8]
            test    rax, 0x1
            jnz     .level2LoopToNext
            ALLOC_FRAME
            mov     rcx, rax
            or      rcx, 0x3
            and     rcx, rdi
            mov     [r10 + r11 * 8], rcx
        .level2LoopToNext:
            and     rax, ~(PAGE_SIZE - 1)
            mov     r8, rax
            xor     r9, r9

            .level1LoopTop:
                cmp     r9, 0x200
                jae     .level1LoopOut
                cmp     rbx, rsi
                jae     .break
                mov     rax, rbx
                or      rax, 0x3
                mov     [r8 + r9 * 8], rax
                mov     [directMapMaxMappedOffset], rbx
                add     rbx, PAGE_SIZE

                inc     r9
                jmp     .level1LoopTop
            .level1LoopOut:

            inc     r11
            jmp     .level2LoopTop
        .level2LoopOut:

        inc     r13
        jmp     .level3LoopTop
    .level3LoopOut:

    inc     r15
    jmp     .level4LoopTop
.level4LoopOut:
.break:

    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    leave
    ret
