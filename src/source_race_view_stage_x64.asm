option casemap:none
.const
source_effect_angle_factor DWORD 041b60b61h ;46EBA0
source_impact_one DWORD 03f800000h ;46E264
source_impact_fifteenth DWORD 03d888889h ;46E564
source_impact_quarter DWORD 03e800000h ;46E24C
.code
PUBLIC sgn_source_effect_angle_45e4b3
sgn_source_effect_angle_45e4b3 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx] ;45E4B3
    fmul DWORD PTR [source_effect_angle_factor] ;45E4B6
    fnstcw WORD PTR [rsp+8] ;462866
    fwait
    mov ax,WORD PTR [rsp+8]
    or ah,0ch
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
    fistp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp]
    add rsp,16
    ret
sgn_source_effect_angle_45e4b3 ENDP
PUBLIC sgn_source_effect_alpha_45e5ad
sgn_source_effect_alpha_45e5ad PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx] ;45E5AD
    fnstcw WORD PTR [rsp+8] ;462866
    fwait
    mov ax,WORD PTR [rsp+8]
    or ah,0ch
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
    fistp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp]
    add rsp,16
    ret
sgn_source_effect_alpha_45e5ad ENDP
PUBLIC sgn_source_impact_scale_41606d
sgn_source_impact_scale_41606d PROC
    cmp DWORD PTR [rcx],15
    jle impact_low_lifetime
    fld DWORD PTR [source_impact_one]
    jmp impact_scale
impact_low_lifetime:
    fild DWORD PTR [rcx] ;41607F
    fmul DWORD PTR [source_impact_fifteenth] ;416082
impact_scale:
    fmul DWORD PTR [rdx] ;416088
    fst DWORD PTR [r8] ;41608B: rounded store, retained extended comparison
    fcomp DWORD PTR [source_impact_quarter]
    fnstsw ax
    test ah,1 ;C0 only, includes unordered
    jz impact_above_quarter
    mov DWORD PTR [r8],03e800000h
    ret
impact_above_quarter:
    fld DWORD PTR [r8] ;4160A4: fresh binary32
    fcomp DWORD PTR [source_impact_one]
    fnstsw ax
    test ah,041h ;C0|C3
    jnz impact_scale_done
    mov DWORD PTR [r8],03f800000h
impact_scale_done:
    ret
sgn_source_impact_scale_41606d ENDP
END
