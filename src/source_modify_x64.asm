; Win64 leaf: RCX three input DWORDs, RDX output, R8D expression0..15.
; Private callers only; no stack, calls, nonvolatile registers or control-word change.
; Expressions end at the original FSTP32; no extra rounding inside chains.
OPTION CASEMAP:NONE
.const
m_46e4f8 DWORD 03c23d70ah
m_46e49c DWORD 041800000h
m_46ebe4 DWORD 043a98000h
m_46e358 DWORD 041a00000h
m_46e1f8 DWORD 041000000h
m_46ebf0 DWORD 042fd70a5h
m_46e1d4 DWORD 043a00000h
m_46ebe8 DWORD 043330000h
m_46e1d8 DWORD 03f000000h
.code
PUBLIC sgn_modify_numeric
sgn_modify_numeric PROC
    cmp r8d,0
    je modify_op_0
    cmp r8d,1
    je modify_op_1
    cmp r8d,2
    je modify_op_2
    cmp r8d,3
    je modify_op_3
    cmp r8d,4
    je modify_op_4
    cmp r8d,5
    je modify_op_5
    cmp r8d,6
    je modify_op_6
    cmp r8d,7
    je modify_op_7
    cmp r8d,8
    je modify_op_8
    cmp r8d,9
    je modify_op_9
    cmp r8d,10
    je modify_op_10
    cmp r8d,11
    je modify_op_11
    cmp r8d,12
    je modify_op_12
    cmp r8d,13
    je modify_op_13
    cmp r8d,14
    je modify_op_14
    jmp modify_op_15
modify_op_0:
    fild DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
modify_op_1:
    fild DWORD PTR [rcx]
    fmul DWORD PTR [m_46e4f8]
    fstp DWORD PTR [rdx]
    ret
modify_op_2:
    fild DWORD PTR [rcx]
    fmul DWORD PTR [m_46e49c]
    fadd DWORD PTR [m_46ebe4]
    fstp DWORD PTR [rdx]
    ret
modify_op_3:
    fld DWORD PTR [rcx]
    fmul DWORD PTR [m_46e358]
    fstp DWORD PTR [rdx]
    ret
modify_op_4:
    fld DWORD PTR [rcx]
    fmul DWORD PTR [m_46e1f8]
    fstp DWORD PTR [rdx]
    ret
modify_op_5:
    fld DWORD PTR [rcx]
    fmul DWORD PTR [m_46ebf0]
    fsubr DWORD PTR [m_46e1d4]
    fstp DWORD PTR [rdx]
    ret
modify_op_6:
    fild DWORD PTR [rcx]
    fsubr DWORD PTR [rcx+4]
    fmul DWORD PTR [m_46ebf0]
    fsubr DWORD PTR [m_46ebe8]
    fstp DWORD PTR [rdx]
    ret
modify_op_7:
    fild DWORD PTR [rcx]
    fsubr DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx+8]
    fmul DWORD PTR [m_46ebf0]
    fsubr DWORD PTR [m_46e1d4]
    fstp DWORD PTR [rdx]
    ret
modify_op_8:
    fld DWORD PTR [rcx]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rdx]
    ret
modify_op_9:
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rcx+4]
    fstp DWORD PTR [rdx]
    ret
modify_op_10:
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rcx+4]
    fstp DWORD PTR [rdx]
    ret
modify_op_11:
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rdx]
    ret
modify_op_12:
    fld DWORD PTR [rcx]
    fadd DWORD PTR [rcx+4]
    fmul DWORD PTR [m_46e1d8]
    fstp DWORD PTR [rdx]
    ret
modify_op_13:
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [m_46e1d8]
    fsubr DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
modify_op_14:
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx+8]
    fmul DWORD PTR [m_46e1d8]
    fsubr DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
modify_op_15:
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx+8]
    fsubr DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
sgn_modify_numeric ENDP
END
