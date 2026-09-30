; Win64 leaf ABI: RCX input word pointer, RDX output word pointer.
; Current x87 control is retained; no nonvolatile register, stack or calls.
OPTION CASEMAP:NONE
.const
select_team_one_46e264 DWORD 03f800000h
select_team_spacing_46ec3c DWORD 0438c0000h
select_team_center_46e1d4 DWORD 043a00000h
.code
PUBLIC sgn_select_team_index_45b831
sgn_select_team_index_45b831 PROC
    fild DWORD PTR [rcx]
    fsub DWORD PTR [select_team_one_46e264]
    fstp DWORD PTR [rdx]
    ret
sgn_select_team_index_45b831 ENDP
PUBLIC sgn_select_team_strip_45ba34
sgn_select_team_strip_45ba34 PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [select_team_spacing_46ec3c]
    fsubr DWORD PTR [select_team_center_46e1d4]
    fstp DWORD PTR [rdx]
    ret
sgn_select_team_strip_45ba34 ENDP
PUBLIC sgn_select_team_next_45ba89
sgn_select_team_next_45ba89 PROC
    fld DWORD PTR [rcx]
    fadd DWORD PTR [select_team_spacing_46ec3c]
    fstp DWORD PTR [rdx]
    ret
sgn_select_team_next_45ba89 ENDP
END
