option casemap:none
.const
uv_zero DWORD 000000000h
uv_one QWORD 03ff0000000000000h
uv_low QWORD 03f50624dd2f1a9fch
uv_high QWORD 03feff7ced916872bh
uv_delta QWORD 03f747ae147ae147bh
.code
;One of the two identical4310A0 coordinate blocks, actual pointer in RCX.
;Finite binary32 admitted by the caller. Modern x87 FPREM provides the
;ordinary462924 leaf; old Pentium workaround/CRT exceptional paths not claimed.
PUBLIC sgn_source_uv_coordinate_4310a0
sgn_source_uv_coordinate_4310a0 PROC
    fld DWORD PTR [rcx]
    fcomp DWORD PTR [uv_zero]
    fnstsw ax
    test ah,1
    jz uv_positive
    fld DWORD PTR [rcx]
    fchs
    jmp uv_remainder
uv_positive:
    fld DWORD PTR [rcx]
uv_remainder:
    fld QWORD PTR [uv_one]
    fxch
uv_reduce:
    fprem
    fstsw ax
    fwait
    test ah,4
    jnz uv_reduce
    fstp st(1)
    fcom QWORD PTR [uv_low]
    fnstsw ax
    test ah,41h
    jz uv_upper
    fld DWORD PTR [rcx]
    fadd QWORD PTR [uv_delta]
    fstp DWORD PTR [rcx]
uv_upper:
    fcomp QWORD PTR [uv_high]
    fnstsw ax
    test ah,1
    jnz uv_done
    fld DWORD PTR [rcx]
    fsub QWORD PTR [uv_delta]
    fstp DWORD PTR [rcx]
uv_done:
    ret
sgn_source_uv_coordinate_4310a0 ENDP
END
