OPTION CASEMAP:NONE
.const
bootstrap_extent_scale DWORD 03F7D70A4h ;pinned46E3B8
.code
PUBLIC sgn_bootstrap_maximum_z
sgn_bootstrap_maximum_z PROC
    fld DWORD PTR [rcx]
    fcom DWORD PTR [rdx]
    fnstsw ax
    test ah,1
    jz keep_maximum
    fstp st(0)
    fld DWORD PTR [rdx]
keep_maximum:
    fstp DWORD PTR [rcx]
    ret
sgn_bootstrap_maximum_z ENDP
PUBLIC sgn_bootstrap_negate_x
sgn_bootstrap_negate_x PROC
    fld DWORD PTR [rcx]
    fchs
    fstp DWORD PTR [rcx]
    ret
sgn_bootstrap_negate_x ENDP
PUBLIC sgn_bootstrap_extent
sgn_bootstrap_extent PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [bootstrap_extent_scale]
    fstp DWORD PTR [rdx]
    ret
sgn_bootstrap_extent ENDP
END
