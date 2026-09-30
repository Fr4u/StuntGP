option casemap:none
.const
scene_one QWORD 03ff0000000000000h ;46E2C8
scene_ambient_scale DWORD 03b808081h ;46E444, pinned below by PE audit
scene_raster_max DWORD 0477f0000h ;46E668
scene_raster_min DWORD 045000000h ;46E5F4
.code
PUBLIC sgn_scene_ambient_4468b4
sgn_scene_ambient_4468b4 PROC
    fild DWORD PTR [rcx]
    fmul DWORD PTR [scene_ambient_scale]
    fstp DWORD PTR [rdx]
    fild DWORD PTR [rcx+4]
    fstp DWORD PTR [rdx+4]
    fild DWORD PTR [rcx+8]
    fstp DWORD PTR [rdx+8]
    fild DWORD PTR [rcx+12]
    fstp DWORD PTR [rdx+12]
    ret
sgn_scene_ambient_4468b4 ENDP

PUBLIC sgn_scene_pow_capture64
sgn_scene_pow_capture64 PROC
    fld QWORD PTR [rdx] ;source base, then exponent
    fld QWORD PTR [r8]
    fnstcw WORD PTR [rcx+20]
    fnstsw WORD PTR [rcx+22]
    fstp TBYTE PTR [rcx]
    fstp TBYTE PTR [rcx+10]
    ret
sgn_scene_pow_capture64 ENDP
PUBLIC sgn_scene_pow_capture32
sgn_scene_pow_capture32 PROC
    fld QWORD PTR [rdx]
    fld DWORD PTR [r8]
    fnstcw WORD PTR [rcx+20]
    fnstsw WORD PTR [rcx+22]
    fstp TBYTE PTR [rcx]
    fstp TBYTE PTR [rcx+10]
    ret
sgn_scene_pow_capture32 ENDP

PUBLIC sgn_scene_pow_reciprocal
sgn_scene_pow_reciprocal PROC
    fld TBYTE PTR [rcx]
    fdivr QWORD PTR [scene_one]
    fstp DWORD PTR [rdx] ;real local FLOAT32 reciprocal
    ret
sgn_scene_pow_reciprocal ENDP

PUBLIC sgn_scene_pow_raster
sgn_scene_pow_raster PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld TBYTE PTR [rcx]
    fmul DWORD PTR [rdx]
    fmul DWORD PTR [scene_raster_max]
    fcom DWORD PTR [scene_raster_min]
    fnstsw ax
    test ah,1
    jz scene_raster_high
    fstp st(0)
    fld DWORD PTR [scene_raster_min]
    jmp scene_raster_convert
scene_raster_high:
    fcom DWORD PTR [scene_raster_max]
    fnstsw ax
    test ah,41h
    jnz scene_raster_convert
    fstp st(0)
    fld DWORD PTR [scene_raster_max]
scene_raster_convert:
    ;actual462860 at4223F7; low DWORD of signed64 result.
    fstcw WORD PTR [rsp+8]
    fwait
    mov ax,WORD PTR [rsp+8]
    or ah,0ch
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
    fistp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp]
    fld QWORD PTR [r9]
    fadd QWORD PTR [scene_one]
    mov DWORD PTR [r8],eax ;422405 before local counter FSTP422410
    fstp QWORD PTR [r9]
    add rsp,16
    ret
sgn_scene_pow_raster ENDP

PUBLIC sgn_scene_pow_curve_first
sgn_scene_pow_curve_first PROC
    fld TBYTE PTR [rcx]
    fmul DWORD PTR [rdx]
    fstp DWORD PTR [r8] ;422473 before iteration FLD422476
    fld QWORD PTR [r9]
    fadd QWORD PTR [scene_one]
    fstp QWORD PTR [r9]
    ret
sgn_scene_pow_curve_first ENDP
PUBLIC sgn_scene_pow_curve_second
sgn_scene_pow_curve_second PROC
    fld TBYTE PTR [rcx]
    fmul DWORD PTR [rdx]
    fstp DWORD PTR [r8] ;4224C2 before iteration FLD4224C4
    fld QWORD PTR [r9]
    fadd QWORD PTR [scene_one]
    fstp QWORD PTR [r9]
    ret
sgn_scene_pow_curve_second ENDP
END
