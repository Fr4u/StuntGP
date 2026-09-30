option casemap:none
.const
enter_step DWORD 03cf5c28fh ;46EBC0=source phase increment
enter_224 DWORD 043600000h ;46EBF4
enter_32 DWORD 042000000h ;46E5FC
enter_96 DWORD 042c00000h ;46E624
enter_256 DWORD 043800000h ;46E2E0
enter_128 DWORD 043000000h ;46E3EC
enter_64 DWORD 042800000h ;46E570
enter_one DWORD 03f800000h ;46E264
enter_200 DWORD 043480000h ;46E378
enter_pi DWORD 040490fdbh ;46E540
enter_20 DWORD 041a00000h ;46E358
enter_80 DWORD 042a00000h ;46E46C
.code
;4546AD..D5, with literal462860 truncation of the duplicated ST0.
;Only the low DWORD of the original signed64 conversion is reloaded as signed32.
PUBLIC sgn_enter_name_fraction
sgn_enter_name_fraction PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fadd DWORD PTR [enter_step]
    fld st(0)
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ax,0c00h
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    fild DWORD PTR [rsp+8]
    fsubr st(0),st(1)
    fstp DWORD PTR [rdx]
    fstp st(0)
    add rsp,24
    ret
sgn_enter_name_fraction ENDP
;Full43D8F0, with its returned ST0 stored by the caller. Includes unordered flags.
PUBLIC sgn_enter_name_linear_43d8f0
sgn_enter_name_linear_43d8f0 PROC
    fld DWORD PTR [rcx]
    fcomp DWORD PTR [rcx+4]
    fnstsw ax
    test ah,40h
    jnz enter_linear_current
    fld DWORD PTR [rcx+4]
    fsub DWORD PTR [rcx]
    fabs
    fld DWORD PTR [rcx+8]
    fcompp
    fld DWORD PTR [rcx+4]
    fnstsw ax
    test ah,1
    jz enter_linear_store
    fcomp DWORD PTR [rcx]
    fld DWORD PTR [rcx+8]
    fnstsw ax
    test ah,1
    jz enter_linear_add
    fchs
enter_linear_add:
    fadd DWORD PTR [rcx]
    jmp enter_linear_store
enter_linear_current:
    fld DWORD PTR [rcx]
enter_linear_store:
    fstp DWORD PTR [rdx]
    ret
sgn_enter_name_linear_43d8f0 ENDP
PUBLIC sgn_enter_name_integer_float
sgn_enter_name_integer_float PROC
    fild DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
sgn_enter_name_integer_float ENDP
;454836..454895. Inputs{x,y}; outputs{left,top,frame_left,frame_top,right,bottom}.
;Keep left and top unspilled for their subsequent subtract32 despite FST32 copies.
PUBLIC sgn_enter_name_layout
sgn_enter_name_layout PROC
    fld DWORD PTR [rcx]
    fsub DWORD PTR [enter_224]
    fld st(0)
    fsub DWORD PTR [enter_32]
    fstp DWORD PTR [rdx+8]
    fld DWORD PTR [rcx+4]
    fsub DWORD PTR [enter_96]
    fst DWORD PTR [rdx+4]
    fsub DWORD PTR [enter_32]
    fstp DWORD PTR [rdx+12]
    fld DWORD PTR [rcx]
    fadd DWORD PTR [enter_256]
    fstp DWORD PTR [rdx+16]
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [enter_128]
    fstp DWORD PTR [rdx+20]
    fstp DWORD PTR [rdx]
    ret
sgn_enter_name_layout ENDP
;Repeated cell coordinate from4548BF/45491D/4549C9/454A3B.
PUBLIC sgn_enter_name_cell
sgn_enter_name_cell PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [enter_64]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rdx]
    ret
sgn_enter_name_cell ENDP
;45492F..454968. Inputs{slide,original_bottom}; outputs{y,top,bottom}.
;FSTs deliberately retain the unrounded intermediate until the final FSTP.
PUBLIC sgn_enter_name_edit_panel
sgn_enter_name_edit_panel PROC
    fld DWORD PTR [enter_one]
    fsub DWORD PTR [rcx]
    fmul DWORD PTR [enter_200]
    fadd DWORD PTR [rcx+4]
    fadd DWORD PTR [enter_64]
    fst DWORD PTR [rdx]
    fsub DWORD PTR [enter_32]
    fst DWORD PTR [rdx+4]
    fadd DWORD PTR [enter_64]
    fstp DWORD PTR [rdx+8]
    ret
sgn_enter_name_edit_panel ENDP
;454A5F..454A92, original FSIN and source float constants.
PUBLIC sgn_enter_name_player_x
sgn_enter_name_player_x PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [enter_pi]
    fsin
    fmul DWORD PTR [enter_20]
    fadd DWORD PTR [enter_80]
    fsubr DWORD PTR [enter_224]
    fstp DWORD PTR [rdx]
    ret
sgn_enter_name_player_x ENDP
END
