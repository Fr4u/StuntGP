; New Windows x64 ABI adapter for439380 arithmetic, retaining caller x87 CW.
.const
fill_zero DWORD 000000000h
fill_one DWORD 03F800000h
fill_alpha_scale DWORD 0437F0000h ;46E1E4=255
fill_row_scale DWORD 042000000h ;46E5FC=32
.code
PUBLIC source_menu_fill_alpha_x87
source_menu_fill_alpha_x87 PROC
    sub rsp,32
    mov dword ptr [rsp],ecx
    ;43D7B0: unordered or below minimum selects minimum.
    fld dword ptr [rsp]
    fcomp dword ptr [fill_zero]
    fnstsw ax
    test ah,1
    jnz use_zero
    fld dword ptr [rsp]
    fcomp dword ptr [fill_one]
    fnstsw ax
    test ah,41h
    jnz use_input
    fld dword ptr [fill_one]
    jmp multiply_alpha
use_zero:
    fld dword ptr [fill_zero]
    jmp multiply_alpha
use_input:
    fld dword ptr [rsp]
multiply_alpha:
    fmul dword ptr [fill_alpha_scale]
    ;462860 truncates to signed64 under a temporary round-toward-zero CW.
    fstcw word ptr [rsp+8]
    fwait
    mov ax,word ptr [rsp+8]
    or ah,0Ch
    mov word ptr [rsp+10],ax
    fldcw word ptr [rsp+10]
    fistp qword ptr [rsp+16]
    fldcw word ptr [rsp+8]
    mov eax,dword ptr [rsp+16]
    add rsp,32
    ret
source_menu_fill_alpha_x87 ENDP
PUBLIC source_menu_fill_geometry_x87
source_menu_fill_geometry_x87 PROC
    sub rsp,8
    mov dword ptr [rsp],r8d
    ;4393F9..439408: signed row converted before the rectangle arithmetic.
    fild dword ptr [rsp]
    mov eax,dword ptr [rdx]
    mov r9d,dword ptr [rdx+4]
    mov dword ptr [rcx+4],eax
    mov dword ptr [rcx+8],r9d
    fmul dword ptr [fill_row_scale]
    fld dword ptr [rdx+8]
    fsub dword ptr [rdx]
    fstp dword ptr [rcx+12]
    fld dword ptr [rdx+12]
    fsub dword ptr [rdx+4]
    fstp dword ptr [rcx+16]
    fld dword ptr [rcx+24]
    fadd st(0),st(1)
    fstp dword ptr [rcx+24]
    fld dword ptr [rcx+32]
    fadd st(0),st(1)
    fstp dword ptr [rcx+32]
    fstp st(0)
    add rsp,8
    ret
source_menu_fill_geometry_x87 ENDP
END
