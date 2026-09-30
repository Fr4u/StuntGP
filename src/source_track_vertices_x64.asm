option casemap:none
.code
; New Win64 ABI leaf for the reached x87 arithmetic of429730.
; RCX=actual61C428 matrix, RDX=XYZ source bytes, R8={px,py,pz,offset,near,stamp},
; R9=actual32-byte transformed vertex. C++ has already written its stamp.
; Keeps the caller's source numeric environment; never resets the x87 CW.
PUBLIC sgn_track_vertex_inside_42979a
sgn_track_vertex_inside_42979a PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+16]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+32]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+48]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+20]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+36]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+52]
    fstp DWORD PTR [rsp]
    fld DWORD PTR [rcx+24]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+40]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+56]
    fld DWORD PTR [r8]
    fmul st(0),st(2)
    fstp DWORD PTR [r9]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rsp]
    fstp DWORD PTR [r9+4]
    fld DWORD PTR [r8+8]
    fmul st(0),st(1)
    fadd DWORD PTR [r8+12]
    fstp DWORD PTR [r9+8]
    fstp DWORD PTR [r9+28]
    fstp st(0)
    add rsp,40
    ret
sgn_track_vertex_inside_42979a ENDP

;4298F1..429A03: clipped X/Y spill, retained projected X/Z,
;actual mask spill/sign-bit order and late stamp/W/mask stores.
PUBLIC sgn_track_vertex_clipped_4298f1
sgn_track_vertex_clipped_4298f1 PROC FRAME
    sub rsp,56
    .allocstack 56
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+16]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+32]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+48]
    fstp DWORD PTR [rsp] ;originalEBP-20, X
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+20]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+36]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+52]
    fstp DWORD PTR [rsp+4] ;originalEBP-1C, Y
    fld DWORD PTR [rcx+24]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+40]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+56] ;cameraZ remains unspilled
    fld DWORD PTR [r8]
    fmul DWORD PTR [rsp]
    fst DWORD PTR [r9]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rsp+4]
    fst DWORD PTR [rsp+8] ;originalEBP-8, projectedY
    fstp DWORD PTR [r9+4]
    fld DWORD PTR [r8+8]
    fmul st(0),st(2)
    fadd DWORD PTR [r8+12]
    fst DWORD PTR [r9+8]
    fld DWORD PTR [r8+16]
    fsub st(0),st(1)
    fstp DWORD PTR [rsp+12] ;near-projectedZ
    fstp st(0)
    fld st(1)
    fchs
    fst DWORD PTR [rsp+16] ;negative cameraZ, separately spilled
    fsub st(0),st(1)
    fstp DWORD PTR [rsp+20] ;-cameraZ-projectedX
    fsub st(0),st(1)
    fstp DWORD PTR [rsp+24] ;projectedX-cameraZ
    fld DWORD PTR [rsp+16]
    fsub DWORD PTR [rsp+8]
    fstp DWORD PTR [rsp+28] ;-cameraZ-projectedY
    fld DWORD PTR [rsp+8]
    fsub st(0),st(1)
    fstp DWORD PTR [rsp+32] ;projectedY-cameraZ
    mov eax,DWORD PTR [rsp+32]
    mov r10d,DWORD PTR [rsp+28]
    shr eax,26
    shr r10d,27
    and eax,32
    and r10d,16
    or eax,r10d
    mov r10d,DWORD PTR [rsp+24]
    shr r10d,28
    and r10d,8
    mov r11d,DWORD PTR [r8+20]
    mov WORD PTR [r9+18],r11w ;4299E5
    or eax,r10d
    mov r10d,DWORD PTR [rsp+20]
    shr r10d,29
    and r10d,4
    or eax,r10d
    mov r10d,DWORD PTR [rsp+12]
    fstp DWORD PTR [r9+28] ;4299F9
    shr r10d,31
    or eax,r10d
    or al,2
    mov WORD PTR [r9+16],ax ;429A03, preserves upper stamp halfword
    add rsp,56
    ret
sgn_track_vertex_clipped_4298f1 ENDP
END
