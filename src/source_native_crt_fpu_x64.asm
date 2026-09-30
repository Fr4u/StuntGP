.const
probe_numerator QWORD 04150017EC0000000h
probe_denominator QWORD 04147FFFF80000000h
probe_zero QWORD 0
.code
PUBLIC sgn_crt_probe_464188
sgn_crt_probe_464188 PROC FRAME
    sub rsp, 24
    .allocstack 24
    .endprolog
    fld QWORD PTR [probe_numerator]
    fdiv QWORD PTR [probe_denominator]
    fmul QWORD PTR [probe_denominator]
    fsubr QWORD PTR [probe_numerator]
    fstp QWORD PTR [rsp]
    fld QWORD PTR [rsp]
    fcomp QWORD PTR [probe_zero]
    fnstsw ax
    test ah, 041h
    setz al
    movzx eax, al
    add rsp, 24
    ret
sgn_crt_probe_464188 ENDP
PUBLIC sgn_crt_precision_464176
sgn_crt_precision_464176 PROC FRAME
    sub rsp, 8
    .allocstack 8
    .endprolog
    fnstcw WORD PTR [rsp]
    mov ax, WORD PTR [rsp]
    and ax, 0FCFFh
    or ax, 0200h
    mov WORD PTR [rsp], ax
    fldcw WORD PTR [rsp]
    fnclex
    add rsp, 8
    ret
sgn_crt_precision_464176 ENDP
END
