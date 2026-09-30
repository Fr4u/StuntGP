option casemap:none
.const
skid_zero DWORD 000000000h ;PE46E1EC
skid_one DWORD 03f800000h ;PE46E264
skid_width DWORD 041c80000h ;PE46E3D0:25
skid_join DWORD 043c80000h ;PE46E278:400
skid_step DWORD 043480000h ;PE46E378:200
.code

;These scalar bridges return the raw bits of an original binary32 spill.
;They never bridge an unspilled x87 intermediate through double/SSE arithmetic.
SKID_SCALAR MACRO symbol,operation
PUBLIC symbol
symbol PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],edx
    fld DWORD PTR [rsp]
    operation DWORD PTR [rsp+4]
    fstp DWORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp+8]
    add rsp,24
    ret
symbol ENDP
ENDM
SKID_SCALAR sgn_source_skid_add,fadd
SKID_SCALAR sgn_source_skid_sub,fsub
SKID_SCALAR sgn_source_skid_mul,fmul

PUBLIC sgn_source_skid_compare
sgn_source_skid_compare PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],edx
    fld DWORD PTR [rsp]
    fcomp DWORD PTR [rsp+4]
    fnstsw ax
    movzx eax,ah
    add rsp,24
    ret
sgn_source_skid_compare ENDP

;Literal44EDA0 control flow;RCX three local DWORDs,RDX local length DWORD.
;FST of length deliberately leaves the extended root for FDIVR.
PUBLIC sgn_source_skid_normalize_44eda0
sgn_source_skid_normalize_44eda0 PROC
    fld DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+4]
    fld DWORD PTR [rcx]
    fld st(0)
    fmul st(0),st(1)
    fld st(2)
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp st(3)
    fstp st(0)
    fstp st(0)
    fcom DWORD PTR [skid_zero]
    fnstsw ax
    test ah,40h
    jz skid_norm_nonzero
    xor eax,eax
    fstp st(0)
    mov DWORD PTR [rdx],eax
    mov DWORD PTR [rcx+8],eax
    mov DWORD PTR [rcx+4],eax
    mov DWORD PTR [rcx],eax
    ret
skid_norm_nonzero:
    fsqrt
    fst DWORD PTR [rdx]
    fdivr DWORD PTR [skid_one]
    fld st(0)
    fmul DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld st(0)
    fmul DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fmul DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
    ret
sgn_source_skid_normalize_44eda0 ENDP

;41CB99..41CC07 with the reached44ED40 inlined without changed spills.
;RCX actual current vehicle.up,RDX actual ring segment.step,R8 local side.
PUBLIC sgn_source_skid_side_41cb99
sgn_source_skid_side_41cb99 PROC
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [rcx+4]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8]
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [rcx]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8+4]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx+4]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8+8]
    ;44ED40 starts here, retains the source C3 zero/unordered branch.
    fld DWORD PTR [r8+8]
    fld DWORD PTR [r8+4]
    fld DWORD PTR [r8]
    fld st(0)
    fmul st(0),st(1)
    fld st(2)
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp st(3)
    fstp st(0)
    fstp st(0)
    fcom DWORD PTR [skid_zero]
    fnstsw ax
    test ah,40h
    jz skid_side_nonzero
    xor eax,eax
    fstp st(0)
    mov DWORD PTR [r8+8],eax
    mov DWORD PTR [r8+4],eax
    mov DWORD PTR [r8],eax
    jmp skid_side_width
skid_side_nonzero:
    fsqrt
    fdivr DWORD PTR [skid_one]
    fld st(0)
    fmul DWORD PTR [r8]
    fstp DWORD PTR [r8]
    fld st(0)
    fmul DWORD PTR [r8+4]
    fstp DWORD PTR [r8+4]
    fmul DWORD PTR [r8+8]
    fstp DWORD PTR [r8+8]
skid_side_width:
    fld DWORD PTR [r8]
    fmul DWORD PTR [skid_width]
    fstp DWORD PTR [r8]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [skid_width]
    fstp DWORD PTR [r8+4]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [skid_width]
    fstp DWORD PTR [r8+8]
    ret
sgn_source_skid_side_41cb99 ENDP

;41CC8C..41CCBC;RCX ordered six DWORD loads. No spill of three differences
;or squared sum before source FCOMP. Return AH after the original pop order.
PUBLIC sgn_source_skid_join_41cc8c
sgn_source_skid_join_41cc8c PROC
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fsub DWORD PTR [rcx+12]
    fld DWORD PTR [rcx+16]
    fsub DWORD PTR [rcx+20]
    fld st(0)
    fmul st(0),st(1)
    fld st(2)
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fcomp DWORD PTR [skid_join]
    fstp st(0)
    fnstsw ax
    fstp st(0)
    test ah,1
    fstp st(0)
    movzx eax,ah
    ret
sgn_source_skid_join_41cc8c ENDP

;41D06F..41D080;FST stores cumulative while FCOMP still uses extended sum.
PUBLIC sgn_source_skid_cumulative_41d06f
sgn_source_skid_cumulative_41d06f PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],edx
    fld DWORD PTR [rcx]
    fadd DWORD PTR [skid_step]
    fst DWORD PTR [rcx]
    fcomp DWORD PTR [rsp]
    fnstsw ax
    movzx eax,ah
    add rsp,24
    ret
sgn_source_skid_cumulative_41d06f ENDP
END
