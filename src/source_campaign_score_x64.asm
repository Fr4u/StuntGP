; Win64 leaf functions, volatile RCX/RDX/R8 only, no calls/stack changes.
; Current x87 control is retained. The FST-without-pop in bounds is deliberate.
OPTION CASEMAP:NONE
.const
campaign_score_gap_46e20c DWORD 040800000h
campaign_score_padding_46e358 DWORD 041a00000h
campaign_score_half_46e1d8 DWORD 03f000000h
campaign_score_center_46ec20 DWORD 043be0000h
campaign_score_bottom_46ebc4 DWORD 043c00000h
.code
PUBLIC sgn_campaign_score_add_45a52a
sgn_campaign_score_add_45a52a PROC
    fld QWORD PTR [rcx]
    fadd DWORD PTR [rdx]
    fstp DWORD PTR [r8]
    ret
sgn_campaign_score_add_45a52a ENDP
PUBLIC sgn_campaign_score_bounds_45a543
sgn_campaign_score_bounds_45a543 PROC
    fild DWORD PTR [rcx]
    fmul DWORD PTR [campaign_score_gap_46e20c]
    fadd DWORD PTR [rcx+4]
    fst DWORD PTR [rdx]
    fadd DWORD PTR [campaign_score_padding_46e358]
    fmul DWORD PTR [campaign_score_half_46e1d8]
    fld DWORD PTR [campaign_score_center_46ec20]
    fsub st(0),st(1)
    fstp DWORD PTR [rdx+4]
    fadd DWORD PTR [campaign_score_bottom_46ebc4]
    fstp DWORD PTR [rdx+8]
    ret
sgn_campaign_score_bounds_45a543 ENDP
PUBLIC sgn_campaign_score_y_45a5ad
sgn_campaign_score_y_45a5ad PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [campaign_score_half_46e1d8]
    fsubr DWORD PTR [campaign_score_center_46ec20]
    fstp DWORD PTR [rdx]
    ret
sgn_campaign_score_y_45a5ad ENDP
PUBLIC sgn_campaign_score_next_45a5ec
sgn_campaign_score_next_45a5ec PROC
    fld QWORD PTR [rcx]
    fadd DWORD PTR [rdx]
    fadd DWORD PTR [campaign_score_gap_46e20c]
    fstp DWORD PTR [r8]
    ret
sgn_campaign_score_next_45a5ec ENDP
END
