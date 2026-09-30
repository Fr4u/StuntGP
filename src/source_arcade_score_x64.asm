option casemap:none
.const
arcade_alpha_scale DWORD 0437f0000h ;PE46E1E4:255
.code
;438759/43875C and full462860. Input ECX is the original alpha word.
;No clamp and no invented FSTP32 between FMUL and signed64 FISTP.
;Windows x64 adapter owns24 stack bytes and restores the caller x87 CW.
PUBLIC source_frontend_sprite_alpha_438759
source_frontend_sprite_alpha_438759 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp+16],ecx
    fld DWORD PTR [rsp+16]
    fmul DWORD PTR [arcade_alpha_scale]
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ax,0c00h
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
    add rsp,24
    ret
source_frontend_sprite_alpha_438759 ENDP
END
