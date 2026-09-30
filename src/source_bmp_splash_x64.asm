option casemap:none
.const
splash_pi DWORD 040490fdbh ;PE46E540
splash_origin DWORD 043700000h ;PE46E1D0,240
.code
;45B669..45B688. Inputs{step,phase}, output fraction. Retain unspilled sum
;across full462860 truncate-to-int64 and signed LOW DWORD reload.
PUBLIC sgn_bmp_splash_fraction_45b669
sgn_bmp_splash_fraction_45b669 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fadd DWORD PTR [rcx+4]
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
sgn_bmp_splash_fraction_45b669 ENDP
;45B6B1..D5 and repeated waves; inputs{phase,amplitude}, one FSTP32.
PUBLIC sgn_bmp_splash_y_45b6b1
sgn_bmp_splash_y_45b6b1 PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [splash_pi]
    fsin
    fmul DWORD PTR [rcx+4]
    fsubr DWORD PTR [splash_origin]
    fstp DWORD PTR [rdx]
    ret
sgn_bmp_splash_y_45b6b1 ENDP
END
