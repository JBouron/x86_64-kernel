%include "tests/tests.inc"
%include "disk.inc"

SECTION .text
BITS    64

; ==============================================================================
; Test for readDiskSectors.
DEF_GLOBAL_FUNC(readDiskSectorsTest):
    push    rbp
    mov     rbp, rsp

    ; Read the 3 mock sectors into the stack.
    sub     rsp, 3 * 512

    EXTERN  bootDriveIndex
    movzx   rdi, BYTE [bootDriveIndex]
    ; RDI = first sector LBA to read = (&mockSectors - 0x7e00) / 512 + 1 =
    ; (&mockSectors - 0x7c00) / 512.
    ; /!\ Only possible because the vaddr of the mock sectors are congruent with
    ; their disk offset. See comment above mockSectors.
    lea     rsi, [mockSectors - 0x7c00]
    shr     rsi, 9
    mov     rdx, 3
    lea     rcx, [rsp]
    call    readDiskSectors
    TEST_ASSERT_EQ  rax, 3

    ; Now compare the sector data read from what is expected.
    lea     rsi, [mockSectors]
    lea     rdi, [rsp]
    mov     rcx, (3 * 512) / 8
    cld
    repe cmpsq
    ; De-alloc sectors from stack.
    add     rsp, 3 * 512
    ; Check if comparison was successful.
    test    rcx, rcx
    jz      .ok
    TEST_FAIL   "readDiskSectors did not read correctly"
.ok:
    TEST_SUCCESS

SECTION .data
; Create a few mock sectors to be read by readDiskSectorsTest. This is possible
; because stage1 has its ORIG set to 0x7e00 which is 512-byte aligned, hence
; the following label's vaddr will be congruent with its disk offset mod 512
; meaning that the mock sectors will fall exactly in their own disk sector.
; Note that those sectors will be loaded into memory twice:
;   * A first time when stage 0 loads stage 1.
;   * A second time when readDiskSectorsTest loads them.
; Therefore the sectors loaded by stage 0 can be used as groundtruth for the
; comparison.
; Fill those sector with random patterns to make sure the read was correct.
ALIGN   512
mockSectors:
mockSector0:
DQ 0x312951f72b246fd3,0x285222ed813f6336,0x84a23188fa9f9274,0x0a9bc7dc141f02c3
DQ 0xe72c76b010ab3d14,0x16050a245ef6c774,0xbc70e7a0c696c516,0x371e116fb5846278
DQ 0xddc03864cf5676af,0x53bac69a97461d44,0x32f9ed9477326ba7,0x93a1b21d661d5e04
DQ 0x20cf3d807baac460,0x542fc78ac4ba39fd,0x977e795a40773ae0,0x4a4e355007086127
DQ 0xc7ec2eec3f9c702b,0x21fa75cc749976c1,0x0c69f1d5969b0c15,0x62f8b9acd76f843c
DQ 0x350caa94c39fa5b5,0xc99fd7c7751a1129,0x3a0a37a634684698,0xa951352c9ce92c35
DQ 0x7a45939d7f00d9c5,0x3cd4b4a5b6d36c48,0x1739447abe2fffc2,0x23f1ab62cdd5ab02
DQ 0x0abe19673d373442,0x25ebf94b0a807a76,0xc4cc3b6b74e7bfa9,0x82362c4daa8ea1dc
DQ 0x38e26680af04879d,0xccfe4251c72619df,0xf2d92e716a68b68a,0x30601072f169d89e
DQ 0x20f248ac9e9f86ae,0xcf318bb5bf2e7241,0xf2179fc952530bdd,0x50e25bbab55a4a60
DQ 0x3db3d6e4881bcd9a,0xe4dc74cdeff38aae,0xe480b3cf091cf433,0x36f53a1c163657ff
DQ 0xec35cac048332058,0x6e50e41b0bae64cc,0x323e9cda8aaf75a1,0x0d524705f47f73b0
DQ 0x3d6df2f0d9b5369b,0xc11063c460109cbf,0xdafde349589778b9,0x4f11b52baac38582
DQ 0x6c4e635b470e5aad,0x4815ed4d5b61aaa4,0x3987bf737d1ef7c1,0xd08e9fb27abf4d40
DQ 0xd262044d879e8d06,0x690df379810c2f1e,0xabc1a06dca88f0b8,0x865c949b7d19a9dd
DQ 0x8eb051058f8d3155,0xd303acef5fd6b555,0xaa52a7ce6562e780,0xf1065db19b8ebecb
mockSector1:
DQ 0xf13ca8d115c774b9,0xa11d61b249692175,0x5e69637f5dd9ec2c,0xc1d94a705e8b6190
DQ 0xf961055c4420daca,0x88d54462c363b958,0xa247a6c496176b09,0x7e3635544ba6c639
DQ 0x02f3039571116dce,0xa3eb6a2add5fdb61,0x3e13d1d381763b16,0x0201d351b3f2a9d2
DQ 0xddaa8218427436a2,0xb8e34b799bf71852,0x43e5415352a5f7ed,0x8c5edee934c298ec
DQ 0xa08ca27e7782582c,0xe284881d04173677,0x07992a0041b7f4e5,0x51fae2e95c4d841c
DQ 0x21ec428fd6d04170,0xd82008db5f753767,0x2d40f0a9b41b9ee4,0x1bb3f88e016132a3
DQ 0x65dd369a19d9614d,0x5791f3659a084870,0x66e315087d40c5df,0xa0dfa32bc0069859
DQ 0xce9ab722033246e2,0xe358dc5547bfacfc,0x3d6633ccf89eb114,0x1ff96db26eeb7201
DQ 0xe41f165e539b1f60,0xff03aea3ef1d3037,0xa5a9c885a15c8f96,0xe21e4ccfd727692c
DQ 0xbd56ad1593e10619,0x76dd52754fcc96db,0xd37ae6113b550306,0x4c532803414508a3
DQ 0x45f8862b7c0ca986,0x8baadf2de7abccf2,0x3a852f74ca34c708,0x2908f84d8445d6e9
DQ 0x74f0360ff2703481,0x0830e0b22a0a71e9,0xa37ee25b84f173c6,0x1be68228299440de
DQ 0x8da917bc23ca6cb0,0xc8c509a0e02157b9,0xb2411d97ae692437,0x2a8afb309ff6cb33
DQ 0x44aa309e46a09139,0xe826b68095dca22a,0x43786c4062752f4a,0x03431606042d8ca8
DQ 0x1ac20553df5dd391,0xe32b3aa67485dd20,0x4fa21472c5df9715,0xc9c661e425517276
DQ 0x44c5eb5a2e57d4f9,0x5ed35c18e8670f80,0xc1a5337003df1a89,0x524d5b3f02c3274d
mockSector2:
DQ 0xcb97a069aa08cab3,0x2e31a104bb6ddfa1,0x1589323362dd66c6,0x33df8f96496e97ba
DQ 0x26753217e523a951,0x844e427aecda681e,0x55cb0aae33acb8f2,0xab347cd9b78728e3
DQ 0x1415df705733a728,0x5da68303649eea88,0x61a60c153c09bad4,0xab2572b0e8dd3226
DQ 0x3820657aa8f374ac,0xc9e232aa7b71990a,0x5a7492b2bf4bdf40,0x401c23d524f40bdb
DQ 0x5e716449abc705a2,0x5105da2814e5d20a,0x986f679d0f0793ee,0xec3fefd51c1cf60d
DQ 0xed5f571f1476f729,0xe797c4c15ba14289,0x14a5b48525f2ac34,0xb77c277c2472a765
DQ 0xcb11eb5bcbc66af8,0x6533ee7ae080baca,0x91fb0f80c2282ed9,0xc81d29c1837f0f63
DQ 0x06a3895424ce0f2d,0x52880f8f40d416c8,0x21da077546899c26,0x689d8ba31bbf542a
DQ 0x0b05541aabee020c,0xa030bf304280eb10,0x5916af5e80ffbbb6,0x78f8d5cff71f3057
DQ 0xfd5eafcc7e7408cb,0xa76fb74aaa4ea8f3,0x3e5f1955b62c5609,0x2907a61fad76c8fd
DQ 0x6be9542d3d80c5ac,0x7b1fd17c8aade9d4,0x3c105be02a621bac,0xd98109de1a65a2b0
DQ 0x9acaa704652a3467,0x12773acddb024530,0xbb35437ce5db002d,0x2be9129b6458aae6
DQ 0x449fd106704de909,0x318bc42e505436e7,0xc93bb7e79b501771,0x39079c948aa37192
DQ 0xbb9e049d90d8b7f8,0x06e613a3724ced62,0x8a9cd5eb44e6518c,0xcc61152831e75bc6
DQ 0x6f93a867f0fc3a23,0xf7c8eebe01a81880,0xd988cc56170ea020,0x7e4bd7027cc1dba0
DQ 0xfa2f0b4ee8fe56b4,0xa93caea040b74cf6,0xbe0390a1709a8658,0xbdae8795d528241d
