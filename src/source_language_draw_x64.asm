option casemap:none
; Source452EE1..452F2C. Inputs={signed index, phase float bits, signed count}.
; Output={x float bits,y float bits}; Win64 leaf, no nonvolatile register use.
; Caller retains the source PC53/nearest environment; this leaf does not reset it.
.const
language_tau DWORD 040C90FDBh ;46E478
language_200 DWORD 043480000h ;46E378
language_68 DWORD 042880000h ;46EBE0
language_130 DWORD 043020000h ;46EB50
language_240 DWORD 043700000h ;46E1D0
.code
PUBLIC sgn_language_flag_xy_452ee1
sgn_language_flag_xy_452ee1 PROC
    fild DWORD PTR [rcx]
    fsub DWORD PTR [rcx+4]
    fmul DWORD PTR [language_tau]
    fidiv DWORD PTR [rcx+8]
    fld st(0)
    fcos
    fmul DWORD PTR [language_200]
    fadd DWORD PTR [language_68]
    fstp DWORD PTR [rdx]
    fsin
    fmul DWORD PTR [language_130]
    fadd DWORD PTR [language_240]
    fstp DWORD PTR [rdx+4]
    ret
sgn_language_flag_xy_452ee1 ENDP
END
