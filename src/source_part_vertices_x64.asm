option casemap:none
.code
; Win64 ABI: RCX=actual61C428, RDX=source XYZ, R8=px/py/pz/offset/near,
; R9=actual32-byte output. CW is retained; no source stamp access/store.
;429544..429650: X/Y spill, unspilled Z, original final W/mask stores.
PUBLIC sgn_source_part_vertex_clipped_429544
sgn_source_part_vertex_clipped_429544 PROC FRAME
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
    fstp DWORD PTR [rsp] ;originalEBP-1C, X
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+20]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+36]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+52]
    fstp DWORD PTR [rsp+4] ;originalEBP-18, Y
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx]
    fld DWORD PTR [rcx+24]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+40]
    fmul DWORD PTR [rdx+8]
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
    or eax,r10d
    mov r10d,DWORD PTR [rsp+20]
    shr r10d,29
    and r10d,4
    or eax,r10d
    mov r10d,DWORD PTR [rsp+12]
    fstp DWORD PTR [r9+28] ;42963D
    shr r10d,31
    or eax,r10d
    or al,2
    mov WORD PTR [r9+16],ax ;42964C, upper halfword retained
    add rsp,56
    ret
sgn_source_part_vertex_clipped_429544 ENDP

;429673..42971A has Y/Z binary32 spills, unspilled X, and W BEFORE XYZ.
PUBLIC sgn_source_part_vertex_inside_429673
sgn_source_part_vertex_inside_429673 PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    fld DWORD PTR [rcx+16]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+32]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fadd DWORD PTR [rcx+48]
    fld DWORD PTR [rcx+20]
    fmul DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+36]
    fmul DWORD PTR [rdx+8]
    faddp st(1),st(0)
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx]
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
    fst DWORD PTR [rsp+4]
    fstp DWORD PTR [r9+28]
    fld DWORD PTR [r8]
    fmul st(0),st(1)
    fstp DWORD PTR [r9]
    fstp st(0)
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rsp]
    fstp DWORD PTR [r9+4]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [rsp+4]
    fadd DWORD PTR [r8+12]
    fstp DWORD PTR [r9+8]
    add rsp,40
    ret
sgn_source_part_vertex_inside_429673 ENDP
END
