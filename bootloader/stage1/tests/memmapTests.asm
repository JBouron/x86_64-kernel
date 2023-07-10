%include "tests/tests.inc"
%include "memmap.inc"

; Create/push a memmap entry on the stack.
; @param %1: The base.
; @param %2: The length.
; @param %3: The type.
%macro CREATE_MEMMAP_ENTRY 3
    sub     rsp, 24
    mov     QWORD [rsp], %1
    mov     QWORD [rsp + 0x8], %2
    mov     QWORD [rsp + 0x10], %3
%endmacro

; ==============================================================================
; Test for the findNextBoundary function.
DEF_GLOBAL_FUNC64(memmapFindNextBoundaryTest):
    push    rbp
    mov     rbp, rsp
    push    rbx

    ; findNextBoundary is declared as global but not exported through memmap.inc
    ; since it is supposed to be a helper function.
    EXTERN  findNextBoundary

    ; Number of entries in the mock map.
    NUM_ENTRIES EQU 6

    ; Create a dummy memory map, non-sorted with some overlaps.
    ; Entry 5: Range [0x10000; 0x11000[
    CREATE_MEMMAP_ENTRY 0x10000, 0x1000, 1
    ; Entry 4: Range [0x10000; 0x11000[
    CREATE_MEMMAP_ENTRY 0x10000, 0x1000, 1
    ; Entry 3: Range [0x1000; 0x2000[
    CREATE_MEMMAP_ENTRY 0x1000, 0x1000, 1
    ; Entry 2: Range [0x500;  0x1500[
    CREATE_MEMMAP_ENTRY 0x500,  0x1000, 1
    ; Entry 1: Range [0x2000; 0xb000[
    CREATE_MEMMAP_ENTRY 0x2000, 0x9000, 1
    ; Entry 0: Range [0x4000; 0x5000[
    CREATE_MEMMAP_ENTRY 0x4000, 0x1000, 1

    ; We are now going to "walk" the boundaries by calling findNextBoundary
    ; repeatedly starting at the offset returned by the previous call + 1.
    ; For reference we are expecting the following results:
    ;   * For offset = 0x0:     rax = 0x500     rdx = {2}
    ;   * For offset = 0x501:   rax = 0x1000    rdx = {3}
    ;   * For offset = 0x1001:  rax = 0x1500    rdx = {2}
    ;   * For offset = 0x1501:  rax = 0x2000    rdx = {1,3}
    ;   * For offset = 0x2001:  rax = 0x4000    rdx = {0}
    ;   * For offset = 0x4001:  rax = 0x5000    rdx = {0}
    ;   * For offset = 0x5001:  rax = 0xb000    rdx = {1}
    ;   * For offset = 0xb001:  rax = 0x10000   rdx = {4,5}
    ;   * For offset = 0x10001: rax = 0x11000   rdx = {4,5}
    ;   * For offset = 0x11001: rax = -1        rdx = {}

    ; RBX = search start offset
    xor     rbx, rbx

    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x500
    TEST_ASSERT_EQ  rdx, (1 << 2)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x1000
    TEST_ASSERT_EQ  rdx, (1 << 3)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x1500
    TEST_ASSERT_EQ  rdx, (1 << 2)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x2000
    TEST_ASSERT_EQ  rdx, ((1 << 3) | (1 << 1))

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x4000
    TEST_ASSERT_EQ  rdx, (1 << 0)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x5000
    TEST_ASSERT_EQ  rdx, (1 << 0)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0xb000
    TEST_ASSERT_EQ  rdx, (1 << 1)

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x10000
    TEST_ASSERT_EQ  rdx, ((1 << 5) | (1 << 4))

    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, 0x11000
    TEST_ASSERT_EQ  rdx, ((1 << 5) | (1 << 4))

    ; No more boundaries.
    mov     rbx, rax
    inc     rbx
    mov     rdi, rsp
    mov     rsi, NUM_ENTRIES
    mov     rdx, rbx
    call    findNextBoundary
    TEST_ASSERT_EQ  rax, -1
    TEST_ASSERT_EQ  rdx, 0

    pop     rbx
    TEST_SUCCESS

; ==============================================================================
; Test for the typeFromBitmap function.
DEF_GLOBAL_FUNC64(memmapTypeFromBitmapTest):
    push    rbp
    mov     rbp, rsp

    ; typeFromBitmap is declared as global but not exported through memmap.inc
    ; since it is supposed to be a helper function.
    EXTERN  typeFromBitmap

    ; Create a dummy map. The ranges don't actually matter as we are only
    ; testing typeFromBitmap. The types are range from 0 to 5 which are not
    ; technically "real" type (although E820's documentation is sparse on that)
    ; but again should not matter for typeFromBitmap.
    ; Entry 5: Range [0x10000; 0x11000[
    CREATE_MEMMAP_ENTRY 0x10000, 0x1000, 5
    ; Entry 4: Range [0x10000; 0x11000[
    CREATE_MEMMAP_ENTRY 0x10000, 0x1000, 4
    ; Entry 3: Range [0x1000; 0x2000[
    CREATE_MEMMAP_ENTRY 0x1000, 0x1000, 3
    ; Entry 2: Range [0x500;  0x1500[
    CREATE_MEMMAP_ENTRY 0x500,  0x1000, 2
    ; Entry 1: Range [0x2000; 0xb000[
    CREATE_MEMMAP_ENTRY 0x2000, 0x9000, 1
    ; Entry 0: Range [0x4000; 0x5000[
    CREATE_MEMMAP_ENTRY 0x4000, 0x1000, 0

    ; Some trivial test case with only a single entry set.
    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, (1 << 0)
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 0

    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, (1 << 2)
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 2

    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, (1 << 5)
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 5

    ; Some more complicated test cases with multiple entries having their bit
    ; set in the input bitmap.
    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, ((1 << 5) | (1 << 2))
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 5

    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, ((1 << 3) | (1 << 2) | (1 << 1))
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 3

    ; Special case: all entries are set.
    mov     rdi, rsp
    mov     rsi, 6
    mov     rdx, ((1 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1) | 1)
    call    typeFromBitmap
    TEST_ASSERT_EQ  rax, 5

    TEST_SUCCESS

; ==============================================================================
; Test for the sanitizeMemMap function.
DEF_GLOBAL_FUNC64(memmapSanitizeMemMapTest):
    push    rbp
    mov     rbp, rsp

    ; sanitizeMemMap is declared as global but not exported through memmap.inc
    ; since it is supposed to be a helper function.
    EXTERN  sanitizeMemMap

    ; Create a map with overlapping regions having different types.
    ; Range [0; 1500[ type = 1
    CREATE_MEMMAP_ENTRY 0, 1500, 1
    ; Range [500; 1500[ type = 2.
    CREATE_MEMMAP_ENTRY 500, 1000, 2
    ; Range [1000; 2000[ type = 1.
    CREATE_MEMMAP_ENTRY 1000, 1000, 1
    ; Range [2000; 3000[ type = 1.
    CREATE_MEMMAP_ENTRY 2000, 1000, 1
    ; Range [3000; 4000[ type = 3.
    CREATE_MEMMAP_ENTRY 3000, 1000, 3
    ; Range [4000; 5000[ type = 1
    CREATE_MEMMAP_ENTRY 4000, 1000, 1
    ; Range [4000; 4300[ type = 2
    CREATE_MEMMAP_ENTRY 4000, 300, 2
    ; Range [4000; 4200[ type = 3
    CREATE_MEMMAP_ENTRY 4000, 200, 3
    ; Range [4000; 4100[ type = 4
    CREATE_MEMMAP_ENTRY 4000, 100, 4
    ; Range [6000; 7000[ type = 1
    CREATE_MEMMAP_ENTRY 6000, 1000, 1
    ; Range [6000; 7000[ type = 2
    CREATE_MEMMAP_ENTRY 6000, 1000, 2
    ; Range [7000; 8000[ type = 1
    CREATE_MEMMAP_ENTRY 7000, 1000, 1
    ; Range [7000; 7500[ type = 2
    CREATE_MEMMAP_ENTRY 7000, 500, 2
    ; Range [7500; 8000[ type = 3
    CREATE_MEMMAP_ENTRY 7500, 500, 3
    ; Range [9000; 10000[ type = 1
    CREATE_MEMMAP_ENTRY 9000, 1000, 1
    ; Range [10000; 11000[ type = 1
    CREATE_MEMMAP_ENTRY 10000, 1000, 1

    ; Give the memory map above, we expect the following sanitized map.
    ;   base = 0        len = 500       type = 1
    ;   base = 500      len = 1000      type = 2
    ;   base = 1500     len = 1500      type = 1
    ;   base = 3000     len = 1000      type = 3
    ;   base = 4000     len = 100       type = 4
    ;   base = 4100     len = 100       type = 3
    ;   base = 4200     len = 100       type = 2
    ;   base = 4300     len = 700       type = 1
    ;   base = 6000     len = 1500      type = 2
    ;   base = 7500     len = 500       type = 3
    ;   base = 9000     len = 2000      type = 2

    mov     rdi, rsp
    mov     rsi, 16
    ; RAX = New memory map.
    ; RDX = New memory map size (num entries).
    call    sanitizeMemMap
    TEST_ASSERT_EQ  rdx, 11
    ; Check each entry. RAX is updated to point to the next entry after every
    ; check.
    %macro  _ASSERT_MAP_ENTRY_EQ 4
    TEST_ASSERT_EQ  QWORD [%1], %2
    TEST_ASSERT_EQ  QWORD [%1 + 0x8], %3
    TEST_ASSERT_EQ  QWORD [%1 + 0x10], %4
    %endmacro

    _ASSERT_MAP_ENTRY_EQ    rax, 0, 500, 1
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 500,  1000, 2
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 1500, 1500, 1
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 3000, 1000, 3
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 4000, 100,  4
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 4100, 100,  3
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 4200, 100,  2
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 4300, 700,  1
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 6000, 1500, 2
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 7500, 500,  3
    add     rax, 24
    _ASSERT_MAP_ENTRY_EQ    rax, 9000, 2000, 1

    TEST_SUCCESS

; ==============================================================================
; Test for the findFirstAvailFrameTest function.
DEF_GLOBAL_FUNC64(findFirstAvailFrameTest):
    push    rbp
    mov     rbp, rsp
    push    r15
    push    r14

    ; Create a mock memmap on the stack.
    ; A couple of available frames with a non-available frame in the middle. All
    ; bases and page-aligned and lengths are multiple of page size.
    CREATE_MEMMAP_ENTRY 0x0, 0x1000, 1
    CREATE_MEMMAP_ENTRY 0x1000, 0x1000, 2
    CREATE_MEMMAP_ENTRY 0x2000, 0x2000, 1

    ; Some entries with non-page-aligned bases or lengths that are not multiple
    ; of page-size.
    CREATE_MEMMAP_ENTRY 0x10001, 0xfff, 1
    CREATE_MEMMAP_ENTRY 0x11000, 0x1000, 2
    CREATE_MEMMAP_ENTRY 0x12000, 0x1000, 1

    CREATE_MEMMAP_ENTRY 0x20fff, 0x1001, 1

    ; Sanitize the mock memory map, mostly because it needs to be sorted.
    ; R15 = New memory map.
    ; R14 = New memory map size (num entries).
    mov     rdi, rsp
    mov     rsi, 7
    call    sanitizeMemMap
    mov     r15, rax
    mov     r14, rdx

    ; Run the test cases.
; Run a test case. Triggers an assert failure if the return value of
; findFirstAvailFrame does not match the expected value.
; @param %1: The start offset for the findFirstAvailFrame call
; @param %2: The expected return value of findFirstAvailFrame.
%macro FIND_FIRST_AVAIL_FRAME_TEST_CASE 2
    mov     rdi, r15
    mov     rsi, r14
    mov     rdx, %1
    call    findFirstAvailFrame
    mov     rcx, %2
    TEST_ASSERT_EQ  rax, rcx
%endmacro

    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x0, 0x0
    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x200, 0x2000
    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x1000, 0x2000
    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x2000, 0x2000
    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x3000, 0x3000

    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x4000, 0x12000

    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x13000, 0x21000

    FIND_FIRST_AVAIL_FRAME_TEST_CASE 0x30000, -1

    pop     r14
    pop     r15
    TEST_SUCCESS
