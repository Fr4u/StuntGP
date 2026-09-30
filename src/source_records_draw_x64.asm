option casemap:none
.const
records_line_step DWORD 041e00000h ;46EC10 =28
records_half DWORD 03f000000h ;46E1D8
records_y_center DWORD 0436b0000h ;46EC0C =235
records_pi DWORD 040490fdbh ;46E540
.code
;45955F..459584: inputs={visible-1, signed truncated scroll, scroll bits}.
PUBLIC sgn_records_first_y_45955f
sgn_records_first_y_45955f PROC
    fild DWORD PTR [rcx]
    fmul DWORD PTR [records_line_step]
    fmul DWORD PTR [records_half]
    fsubr DWORD PTR [records_y_center]
    fild DWORD PTR [rcx+4]
    fsubr DWORD PTR [rcx+8]
    fmul DWORD PTR [records_line_step]
    fsubp st(1),st(0)
    fstp DWORD PTR [rdx]
    ret
sgn_records_first_y_45955f ENDP
;4595E8..459609: inputs={phase bits, odd row, odd-phase bits, amplitude bits}.
;The source numeric setup remains owned by the existing cold startup.
PUBLIC sgn_records_wave_4595e8
sgn_records_wave_4595e8 PROC
    fld DWORD PTR [rcx]
    cmp DWORD PTR [rcx+4],0
    je records_even
    fadd DWORD PTR [rcx+8]
records_even:
    fmul DWORD PTR [records_pi]
    fsin
    fmul DWORD PTR [rcx+12]
    fstp DWORD PTR [rdx]
    ret
sgn_records_wave_4595e8 ENDP
END
