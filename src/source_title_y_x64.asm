; Original 45C1EB..45C210, pointer arguments use the Windows x64 ABI.
; Leaf function: no stack allocation or nonvolatile register modifications.
.const
title_pi DWORD 040490FDBh
title_thirty DWORD 041F00000h
title_height DWORD 043F00000h
title_origin_y DWORD 042A80000h
.code
PUBLIC sgn_title_y_45c1eb
sgn_title_y_45c1eb PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [title_pi]
    fsin
    fmul DWORD PTR [title_thirty]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [title_height]
    faddp st(1), st(0)
    fsubr DWORD PTR [title_origin_y]
    fstp DWORD PTR [rdx]
    ret
sgn_title_y_45c1eb ENDP
END
