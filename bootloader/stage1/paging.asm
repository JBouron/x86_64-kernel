; Paging related functions.

%include "macros.mac"
%include "framealloc.inc"

PAGE_SIZE   EQU 0x1000

; ==============================================================================
; Helper function for mapPage.
; @param %RDI: Virtual address of the page to be mapped. Must be page aligned.
; @param %RSI: Physical address to map the page to. Must be page aligned.
; @param %RDX: Level.
; @param %RCX: Current table.
DEF_LOCAL_FUNC64(doMapPage):
    push    rbp
    mov     rbp, rsp
    
    push    r15
    push    r14
    push    r13
    push    r12
    
    ; R15 = current table.
    mov     r15, rcx
    ; R14 = current level.
    mov     r14, rdx
    ; R13 = paddr.
    mov     r13, rsi
    ; R12 = vaddr.
    mov     r12, rdi

    ; RAX = Index of entry in current table.
    mov     rax, r12
    mov     rcx, r14
    imul    rcx, 9
    add     rcx, 12
    shr     rax, cl
    and     rax, 0x1ff

    ; RAX = Pointer to entry in current table.
    lea     rax, [r15 + rax * 8]

    ; Check if we are at the last level.
    test    r14, r14
    jnz     .notBaseCase
    
    ; Point the physical page frame.
    or      r13, 0x1
    mov     [rax], r13
    jmp     .out

.notBaseCase:
    ; This is not the base case, we need to recurse to the next level.
    ; Check if the current entry is present, if it is we can directly recurse to
    ; the next level. Otherwise we need to allocate the next level first.
    test    QWORD [rax], 0x1
    jnz     .nextLevelPresent
    ; Next level is not present, allocate it.
    push    rax
    call    allocFrame
    ; RCX = New frame.
    mov     rcx, rax
    pop     rax
    ; Update the current entry to point to the new frame and set the present
    ; bit.
    or      rcx, 0x1
    mov     [rax], rcx
.nextLevelPresent:
    ; RAX still points to current entry at this point. 
    ; Recurse to the next level.
    ; RCX = next table.
    mov     rdi, r12
    mov     rsi, r13
    lea     rdx, [r14 - 1]
    mov     rcx, [rax]
    and     rcx, ~(PAGE_SIZE - 1)
    call    doMapPage

.out:
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    leave
    ret

; ==============================================================================
; Map a virtual frame to a physical page frame.
; FIXME: Add parameter for access bits.
; @param %RDI: Virtual address of the page to be mapped. Must be page aligned.
; @param %RSI: Physical address to map the page to. Must be page aligned.
DEF_GLOBAL_FUNC64(mapPage):
    push    rbp
    mov     rbp, rsp

    ; Check that the addresses are page aligned.
    test    rdi, (PAGE_SIZE - 1)
    jz      .rdiOk
    PANIC   "First arg to mapPage is not a page-aligned vaddr: $", rdi
.rdiOk:
    test    rsi, (PAGE_SIZE - 1)
    jz      .rsiOk
    PANIC   "Second arg to mapPage is not a page-aligned vaddr: $", rsi
.rsiOk:

    ; RCX = Phy address of PML4.
    mov     rcx, cr3
    and     rcx, ~(PAGE_SIZE - 1)
    mov     rdx, 3
    call    doMapPage

    leave
    ret


; ==============================================================================
; Allocate virtual memory mapping to newly allocated physical memory.
; @param %RDI: Start of the virtual memory range to allocate. Must be
; page-aligned.
; @parma %RSI: Number of virtual pages to allocate.
DEF_GLOBAL_FUNC64(allocVirtMem):
    push    rbp
    mov     rbp, rsp
    push    r15
    push    r14

    DEBUG   "Alloc virt mem: start = $ num_pages = $", rdi, rsi

    ; Check that the vaddr is page aligned.
    test    rdi, (PAGE_SIZE - 1)
    jz      .rdiOk
    PANIC   "First arg to allocVirtMem is not a page-aligned vaddr: $", rdi
.rdiOk:

    ; R15 = Next vaddr to map.
    mov     r15, rdi
    ; R14 = Number of pages left to map.
    mov     r14, rsi
.loopCond:
    test    r14, r14
    jz      .loopOut
.loopTop:
    ; Allocate a physical page frame for this new virtual page.
    ; RAX = physical page frame offset.
    call    allocFrame
    ; Map the current vaddr to this new physical frame.
    mov     rdi, r15
    mov     rsi, rax
    call    mapPage
.loopNextIte:
    add     r15, PAGE_SIZE
    dec     r14
.loopOut:

    pop     r14
    pop     r15
    leave
    ret
