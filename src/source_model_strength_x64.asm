option casemap:none
.code
;RCX points at the actual FLOAT strength; RDX holds the two pinned PE DWORDs.
;No intermediate binary32 spill, no replacement by multiply1 or division255.
PUBLIC sgn_source_light_strength_42e3bd
sgn_source_light_strength_42e3bd PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fmul DWORD PTR [rdx+4]
    fstp DWORD PTR [rcx]
    ret
sgn_source_light_strength_42e3bd ENDP
END
