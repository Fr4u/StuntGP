option casemap:none
.const
source_light_radius DWORD 040980000h ;PE46E6FC,4.75
.code
;Full462860 numeric body. Preserve precision/masks/rounding and unspilled
;x87 values; use the signed64 conversion's LOW DWORD, including invalid0.
source_light_ftol MACRO
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ax,0c00h
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
ENDM
PUBLIC sgn_source_light_record_42d756
sgn_source_light_record_42d756 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    cmp DWORD PTR [rcx],1
    jne scaled_color
    cmp DWORD PTR [rcx+4],1
    jne scaled_color
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [source_light_radius]
    fld st(0)
    fmul st(0),st(1)
    fstp DWORD PTR [rcx+8]
    fstp st(0)
    jmp light_done
scaled_color:
    fld DWORD PTR [rcx+6ch]
    fmul DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+70h]
    fmul DWORD PTR [rcx+8]
    fstp DWORD PTR [rsp+16]
    fld DWORD PTR [rcx+68h]
    fmul DWORD PTR [rcx+8]
    source_light_ftol
    mov DWORD PTR [rcx+78h],eax
    source_light_ftol
    fld DWORD PTR [rsp+16]
    mov DWORD PTR [rcx+7ch],eax
    source_light_ftol
    mov DWORD PTR [rcx+80h],eax
light_done:
    add rsp,24
    ret
sgn_source_light_record_42d756 ENDP
PUBLIC sgn_source_light_ambient_42d7e0
sgn_source_light_ambient_42d7e0 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rcx]
    fst DWORD PTR [rsp+16]
    fld DWORD PTR [rcx+12]
    fmul DWORD PTR [rcx]
    fstp DWORD PTR [rsp+20]
    mov eax,DWORD PTR [rsp+20]
    fld st(1)
    mov DWORD PTR [rcx+24],eax
    fstp DWORD PTR [rcx+16]
    fstp DWORD PTR [rcx+20]
    source_light_ftol
    fld DWORD PTR [rsp+16]
    mov r8d,eax
    source_light_ftol
    fld DWORD PTR [rsp+20]
    mov r9d,eax
    source_light_ftol
    shl r9d,8
    shl r8d,8
    mov DWORD PTR [rcx+32],r9d
    mov DWORD PTR [rcx+28],r8d
    shl eax,8
    mov DWORD PTR [rcx+36],eax
    add rsp,24
    ret
sgn_source_light_ambient_42d7e0 ENDP
END
