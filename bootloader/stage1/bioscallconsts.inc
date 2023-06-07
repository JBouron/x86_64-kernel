; Constants used by the bioscall.asm file. No need to include this directly,
; include bioscall.inc instead which already includes this file.
%ifndef BIOSCALLCONSTS_INC
%define BIOSCALLCONSTS_INC

; Field offsets for a BIOSCallPacket (BCP) structure, used in callBiosFunc.
; Fields that are only used as input are named BCP_IN_*, those only used as
; output are named BCP_OUT_* and those which are used for both are named
; BCP_INOUT_*.
BCP_IN_INT_OFF      EQU 0x00 ; Size: 1 Desc: Interrupt number.
BCP_OUT_JC          EQU 0x01 ; Size: 1 Desc: Output value of CF in FLAGS reg.
BCP_INOUT_EAX_OFF   EQU 0x02 ; Size: 4 Desc: Input/output value for EAX.
BCP_INOUT_EBX_OFF   EQU 0x06 ; Size: 4 Desc: Input/output value for EBX.
BCP_INOUT_ECX_OFF   EQU 0x0a ; Size: 4 Desc: Input/output value for ECX.
BCP_INOUT_EDX_OFF   EQU 0x0e ; Size: 4 Desc: Input/output value for EDX.
BCP_INOUT_EDI_OFF   EQU 0x12 ; Size: 4 Desc: Input/output value for EDI.
BCP_INOUT_ESI_OFF   EQU 0x16 ; Size: 4 Desc: Input/output value for ESI.
BCP_INOUT_EBP_OFF   EQU 0x1a ; Size: 4 Desc: Input/output value for EBP.
; Size of a BCP structure.
BCP_SIZE            EQU 0x1e
; For convenience the following constants provide some shortcuts to access half
; registers:
BCP_INOUT_AX_OFF    EQU BCP_INOUT_EAX_OFF
BCP_INOUT_AL_OFF    EQU BCP_INOUT_AX_OFF
BCP_INOUT_AH_OFF    EQU (BCP_INOUT_AX_OFF + 1)
BCP_INOUT_BX_OFF    EQU BCP_INOUT_EBX_OFF
BCP_INOUT_BL_OFF    EQU BCP_INOUT_BX_OFF
BCP_INOUT_BH_OFF    EQU (BCP_INOUT_BX_OFF + 1)
BCP_INOUT_CX_OFF    EQU BCP_INOUT_ECX_OFF
BCP_INOUT_CL_OFF    EQU BCP_INOUT_CX_OFF
BCP_INOUT_CH_OFF    EQU (BCP_INOUT_CX_OFF + 1)
BCP_INOUT_DX_OFF    EQU BCP_INOUT_EDX_OFF
BCP_INOUT_DL_OFF    EQU BCP_INOUT_DX_OFF
BCP_INOUT_DH_OFF    EQU (BCP_INOUT_DX_OFF + 1)
BCP_INOUT_DI_OFF    EQU BCP_INOUT_EDI_OFF
BCP_INOUT_SI_OFF    EQU BCP_INOUT_ESI_OFF
BCP_INOUT_BP_OFF    EQU BCP_INOUT_EBP_OFF

%endif
; vim:syntax=asm filetype=asm
