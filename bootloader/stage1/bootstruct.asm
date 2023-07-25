; The bootstruct is a data structure created by the stage1 bootloader that is
; passed to the kernel.
; This structure contains information such as:
;   - The e820 memory map.
;   - The free-list of physical page frames.
;   - ...
;
; Upon jumping to the kernel's entry point, the bootloader writes the address of
; the bootstruct into the %RDI register.

; The offsets of the fields in the bootstruct. Note: All pointers in the
; bootstruct are pointing to low addresses <= 1MiB. Since addresses <= 1MiB are
; ID mapped, those pointers have vaddr == paddr.
; - memmap_addr
;       Type: QWORD
;       Desc: Holds a pointer to an array of
;               struct { u64 base; u64 len; u64 type}
;             This array represents the memory map returned by e820 and
;             sanitized, e.g. no overlaps and ordered by base address.
BS_MEMMAP_ADDR_OFF          EQU 0x0
; - memmap_len
;       Type: QWORD
;       Desc: The length of the memmap_addr array in number of entries.
BS_MEMMAP_LEN_OFF           EQU 0x8
; - phy_frame_free_list_head
;       Type: QWORD
;       Desc: Pointer to the head of the physical frame free list. Each node in
;       this list represents a memory region available for use with frame/page
;       granularity. Each node has the following format:
;           struct {
;               u64 base
;               u64 num_pages
;               u64 next
;           }
;       `base` is the physical address of the first frame in the region, this is
;       a page-aligned address. `num_pages` is the number of free pages/frames
;       in this region. `next` is a pointer (physical address) to the next node
;       in the free list; set to NULL if this is the last node.
BS_PHY_FRAME_FREE_LIST_HEAD EQU 0x10

; Size of the bootstruct, for use by stage1 only, not stored in bootstruct.
_BS_SIZE                    EQU 0x18

%include "macros.mac"
%include "malloc.inc"
%include "memmap.inc"
%include "framealloc.inc"

; ==============================================================================
; Allocate a bootstruct on the heap and initializes it's content.
; @return %RAX: The address of the allocated bootstruct.
DEF_GLOBAL_FUNC64(createBootStruct):
    push    rbp
    mov     rbp, rsp

    ; RAX = Pointer to allocated bootstruct.
    mov     rdi, _BS_SIZE
    call    malloc

    ; Fill the bootstruct.
    mov     rcx, [memoryMap]
    mov     [rax + BS_MEMMAP_ADDR_OFF], rcx
    mov     rcx, [memoryMapLen]
    mov     [rax + BS_MEMMAP_LEN_OFF], rcx
    mov     rcx, [freeListHead]
    mov     [rax + BS_PHY_FRAME_FREE_LIST_HEAD], rcx

    leave
    ret
