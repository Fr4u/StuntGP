option casemap:none
; Same FILD/FSUBR/FMUL/FSUBR/FSTP sequence at4528C6 and4528DE.
; Inputs={signed index,phase float bits,center float bits}; output float bits.
; Retains caller's x87 environment; no nonvolatile registers or owner changes.
.const
catalog_spacing DWORD 043666667h ;PE46EBDC
.code
PUBLIC sgn_catalog_coordinate_4528c6
sgn_catalog_coordinate_4528c6 PROC
    fild DWORD PTR [rcx]
    fsubr DWORD PTR [rcx+4]
    fmul DWORD PTR [catalog_spacing]
    fsubr DWORD PTR [rcx+8]
    fstp DWORD PTR [rdx]
    ret
sgn_catalog_coordinate_4528c6 ENDP
END
