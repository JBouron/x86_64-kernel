; Long-mode related functions.

%include "macros.mac"
%include "framealloc.inc"

PAGE_SIZE               EQU 0x1000
CR4_PAE_BIT_MASK        EQU (1 << 5)
CR0_PG_BIT_MASK         EQU (1 << 31)
IA32_EFER_MSR           EQU 0xC0000080
IA32_EFER_LME_BIT_MASK  EQU (1 << 8)
IA32_EFER_LMA_BIT_MASK  EQU (1 << 10)


SECTION .data
; The physical address of the top level table of the paging structure to use
; when enabling long mode. Initilialized upon calling jumpToLongMode for the
; first time then re-used for every call to jumpToLongMode.
; This paging structure only ID maps the first 1MiB of physical memory.
longModePageTables:
DD  0x0

SECTION .text

BITS    32

; ==============================================================================
; Create the 64-bit paging structures to identity-map the first 1MiB of physical
; memory.
; @return: The physical address of the PML4, ready to be loaded in CR3.
createIdentityMapLowMem:
    push    ebp
    mov     ebp, esp
    push    ebx

    ; Mapping only the first 1MiB of memory is pretty simple it only requires
    ; exactly one structure at each of the 4 levels. At each level excepted the
    ; page-table level, only the first entry needs to be set, pointing to the
    ; single next-level structure.
    ; The last level, e.g. the page-table, needs to have more than one entry set
    ; since a single entry only covers 4KiB.

    ; Allocate a frame for the PML4.
    call    allocFrameLowMem
    ; EBX = PML4 physical address.
    mov     ebx, eax

    ; Save address of PML4.
    push    eax

    ; Allocate each level down to the page-table level (excluded) and set the
    ; first entry to point to the next level.
    mov     ecx, 3
    ; EBX = Iteration var = pointer to current level table.
.allocLevels:
    ; Allocat next level
    push    ecx
    call    allocFrameLowMem
    pop     ecx
    ; Write the first entry of the current level to point to the next level.
    mov     DWORD [ebx], 3
    or      [ebx], eax
    mov     ebx, eax
    loop    .allocLevels

    ; At this point EBX points to the page-table, e.g. lower level.
    ; Now populate this last-level, which requires more that a single entry
    ; since each entry only covers 4KiB bytes. We therefore need to fill:
    ;   1MiB / 4KiB = 2^20 / 2^12 = 2^8 = 256 entries
    ; aka. half the entries of the page-table.
    
    ; EAX = Physical address to map, e.g. frame offset.
    mov     eax, 0x0
    ; EBX = Pointer to curr entry to modify. Already pointing to the first entry
    ; at this point.
.writePageTableEntries:
    sub     eax, ecx
    ; Write the entry.
    mov     DWORD [ebx], 3
    or      [ebx], eax
    ; Move writing pointer.
    add     ebx, 8
    ; Go to map next frame.
    add     eax, PAGE_SIZE
    cmp     eax, (1 << 20)
    jb      .writePageTableEntries

    ; EAX = Physical address of PML4.
    pop     eax

    pop     ebx
    leave
    ret

; ==============================================================================
; Jump to 64-bit long mode. This function DOES NOT RETURN. The state of the
; stack on the target 64-bit code is as it was before pushing the parameter of
; this function.
; @param (DWORD): Target address. This is expected to point to 64-bit code.
DEF_GLOBAL_FUNC(jumpToLongMode):
    mov     eax, [longModePageTables]
    test    eax, eax
    jnz     .doJump
    ; Need to allocate the paging structures first.
    call    createIdentityMapLowMem
    mov     [longModePageTables], eax
.doJump:
    ; At this point EAX contains the physical address of the PML4. We are ready
    ; to initialize IA-32e mode.

    ; Step 1: Disable paging.
    ; This is not needed since we are not using paging when in 32-bit mode.

    ; Step 2: Enable Physica Address Extension (PAE) in CR4.
    mov     ecx, cr4
    or      ecx, CR4_PAE_BIT_MASK
    mov     cr4, ecx

    ; Step 3: Load CR3 with the physical address of the PML4.
    mov     cr3, eax

    ; Step 4: Set IA32_EFER.LME to 1.
    mov     ecx, IA32_EFER_MSR
    rdmsr
    or      eax, IA32_EFER_LME_BIT_MASK
    wrmsr

    ; Step 5: Enable paging.
    mov     eax, cr0
    or      eax, CR0_PG_BIT_MASK
    mov     cr0, eax

    ; We are now in IA-32e mode. Check the IA32_EFER.LMA bit to make sure.
    mov     ecx, IA32_EFER_MSR
    rdmsr
    test    eax, IA32_EFER_LMA_BIT_MASK
    jnz     .jumpToIa32eOk
    ; Failed.
    PANIC   "Failed to enable IA-32e mode"

.jumpToIa32eOk:
    ; Switch to IA-32e mode was successful now jump to actual long mode.
    jmp     SEG_SEL(5):.longModeTarget
    
BITS    64
.longModeTarget:
    ; We are now running in 64-bit. Get the address where to jump to.
    ; RDX = Address to jump to.
    mov     edx, DWORD [rsp + 4]

    ; Fixup the stack to remove the parameter and the return address to the
    ; caller.
    add     rsp, 8

    ; Jump to target.
    jmp     rdx
