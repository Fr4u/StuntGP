option casemap:none
.code
; RCX=actual forward5DA200, RDX=actual up5DA1F0, R8=actual right5DA1E0.
; All source up stores already occurred. CW retained; XYZ stores only.
PUBLIC sgn_source_sprite_cross_43462f
sgn_source_sprite_cross_43462f PROC
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx+8]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8]
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx+8]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx+8]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8+4]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx+4]
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx+4]
    fsubp st(1),st(0)
    fstp DWORD PTR [r8+8]
    ret
sgn_source_sprite_cross_43462f ENDP

; RCX=6 borrowed pointers: sprite, rectangle, right, up, forward, camera matrix.
; RDX=actual output XYZ. Source arithmetic4346CD..4347B4, no W write.
PUBLIC sgn_source_sprite_vertex_4346cd
sgn_source_sprite_vertex_4346cd PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    mov r10,rcx
    mov r11,rdx
    mov rdx,QWORD PTR [r10+8]
    mov rcx,QWORD PTR [r10]
    mov r8,QWORD PTR [r10+16]
    mov r9,QWORD PTR [r10+24]
    mov rax,QWORD PTR [r10+32]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx+40]
    fld DWORD PTR [rcx+44]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [rcx+48]
    fld DWORD PTR [rax]
    fmul st(0),st(1)
    fld DWORD PTR [r9]
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld DWORD PTR [r8]
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp DWORD PTR [rsp]
    fld DWORD PTR [rax+4]
    fmul st(0),st(1)
    fld DWORD PTR [r8+4]
    fmul st(0),st(4)
    faddp st(1),st(0)
    fld DWORD PTR [r9+4]
    fmul st(0),st(3)
    faddp st(1),st(0)
    fstp DWORD PTR [rsp+4]
    fld DWORD PTR [r8+8]
    fmulp st(3),st(0)
    fld DWORD PTR [rax+8]
    fmul st(0),st(1)
    faddp st(3),st(0)
    fld DWORD PTR [r9+8]
    fmul st(0),st(2)
    faddp st(3),st(0)
    fstp st(0)
    fstp st(0)
    mov r8,QWORD PTR [r10+40]
    fld st(0)
    fmul DWORD PTR [r8+32]
    fld DWORD PTR [rsp+4]
    fmul DWORD PTR [r8+16]
    faddp st(1),st(0)
    fld DWORD PTR [rsp]
    fmul DWORD PTR [r8]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp+8]
    fld DWORD PTR [rsp]
    fmul DWORD PTR [r8+4]
    fld st(1)
    fmul DWORD PTR [r8+36]
    faddp st(1),st(0)
    fld DWORD PTR [rsp+4]
    fmul DWORD PTR [r8+20]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp+12]
    fld DWORD PTR [rsp]
    fmul DWORD PTR [r8+8]
    fxch st(1)
    fmul DWORD PTR [r8+40]
    faddp st(1),st(0)
    fld DWORD PTR [rsp+4]
    fmul DWORD PTR [r8+24]
    faddp st(1),st(0)
    fld DWORD PTR [rsp+8]
    fadd DWORD PTR [rcx+24]
    fstp DWORD PTR [r11]
    fld DWORD PTR [rsp+12]
    fadd DWORD PTR [rcx+28]
    fstp DWORD PTR [r11+4]
    fadd DWORD PTR [rcx+32]
    fstp DWORD PTR [r11+8]
    add rsp,40
    ret
sgn_source_sprite_vertex_4346cd ENDP
END
