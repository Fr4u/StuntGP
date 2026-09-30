option casemap:none
.code
PUBLIC sgn_source_trail_vertex_4344ad
sgn_source_trail_vertex_4344ad PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov r8,QWORD PTR [rcx]
    mov r9,QWORD PTR [rcx+8]
    mov r10,QWORD PTR [rcx+16]
    fld DWORD PTR [r9] ;4344AD: actual rectangle X
    fmul DWORD PTR [r8+10h]
    fld DWORD PTR [r8+14h]
    fmul DWORD PTR [r9+4]
    fld DWORD PTR [r9+8] ;actual mutable rectangle Z, never fabricated zero
    fmul DWORD PTR [r8+18h]
    fld st(2)
    fmul DWORD PTR [r10]
    fld st(1)
    fmul DWORD PTR [r10+20h]
    faddp st(1),st(0)
    fld st(2)
    fmul DWORD PTR [r10+10h]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp] ;4344F0 X binary32 store
    fld st(2)
    fmul DWORD PTR [r10+4]
    fld st(1)
    fmul DWORD PTR [r10+24h]
    faddp st(1),st(0)
    fld st(2)
    fmul DWORD PTR [r10+14h]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp+4] ;434506 Y binary32 store
    fmul DWORD PTR [r10+28h]
    fxch st(1)
    fmul DWORD PTR [r10+18h]
    faddp st(1),st(0)
    fxch st(1)
    fmul DWORD PTR [r10+8]
    faddp st(1),st(0)
    fld DWORD PTR [rsp]
    fadd DWORD PTR [r8]
    fstp DWORD PTR [rdx] ;43451F
    fld DWORD PTR [rsp+4]
    fadd DWORD PTR [r8+4]
    fstp DWORD PTR [rdx+4] ;434527
    fadd DWORD PTR [r8+8]
    fstp DWORD PTR [rdx+8] ;43452D. DestinationW untouched.
    add rsp,16
    ret
sgn_source_trail_vertex_4344ad ENDP
END
