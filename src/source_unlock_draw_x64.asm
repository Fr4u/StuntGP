option casemap:none
.const
unlock_width DWORD 044200000h ;46E758:640
unlock_center DWORD 043a00000h ;46E1D4:320
unlock_scale DWORD 03fa00000h ;45E006:1.25
unlock_font_height DWORD 041800000h ;46E49C:16
unlock_line_factor DWORD 03f59999ah ;4793D8:0.85
unlock_padding DWORD 041a00000h ;46E358:20
unlock_half_width DWORD 043910000h ;46EBAC:290
unlock_half DWORD 03f000000h ;46E1D8:0.5
unlock_top DWORD 043ae0000h ;46EC58:348
unlock_bottom DWORD 043b00000h ;46EC34:352
.code
;45DFC5..45DFF1, inputs={row index, page scroll bits}.
PUBLIC sgn_unlock_x_45dfc5
sgn_unlock_x_45dfc5 PROC
    fild DWORD PTR [rcx]
    fsub DWORD PTR [rcx+4]
    fmul DWORD PTR [unlock_width]
    fadd DWORD PTR [unlock_center]
    fstp DWORD PTR [rdx]
    ret
sgn_unlock_x_45dfc5 ENDP
;438950/438A60 height continues unspilled through45E01A..45E05F.
;inputs={x bits, line count}; output={left,top,right,bottom} float bits.
PUBLIC sgn_unlock_rect_45e006
sgn_unlock_rect_45e006 PROC
    fld DWORD PTR [unlock_scale]
    fmul DWORD PTR [unlock_font_height]
    fmul DWORD PTR [unlock_line_factor]
    fimul DWORD PTR [rcx+4]
    fadd DWORD PTR [unlock_padding]
    fld DWORD PTR [rcx]
    fsub DWORD PTR [unlock_half_width]
    fstp DWORD PTR [rdx]
    fld DWORD PTR [rcx]
    fadd DWORD PTR [unlock_half_width]
    fstp DWORD PTR [rdx+8]
    fmul DWORD PTR [unlock_half]
    fld DWORD PTR [unlock_top]
    fsub st(0),st(1)
    fstp DWORD PTR [rdx+4]
    fadd DWORD PTR [unlock_bottom]
    fstp DWORD PTR [rdx+12]
    ret
sgn_unlock_rect_45e006 ENDP
END
