;Windows x64 leaf routines. Input/output are separate DWORD arrays.
;Preserve source FSIN and intermediate x87 arithmetic, PC53/nearest domain.
.const
selection_pi DWORD 040490FDBh
selection_twenty DWORD 041A00000h
selection_120 DWORD 042F00000h
selection_140 DWORD 0430C0000h
selection_half DWORD 03F000000h
selection_100 DWORD 042C80000h
selection_200 DWORD 043480000h
.code
PUBLIC sgn_selection_player_x
sgn_selection_player_x PROC
    cmp DWORD PTR [rcx+8],0
    jne track_player
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [selection_pi]
    fsin
    fmul DWORD PTR [selection_twenty]
    fadd DWORD PTR [selection_120]
    fsubr DWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
track_player:
    fld DWORD PTR [rcx]
    fsub DWORD PTR [selection_140]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [selection_pi]
    fsin
    fmul DWORD PTR [selection_twenty]
    fsubp st(1),st(0)
    fstp DWORD PTR [rdx]
    ret
sgn_selection_player_x ENDP
PUBLIC sgn_selection_frame_xy
sgn_selection_frame_xy PROC
    fld DWORD PTR [rcx]
    fadd DWORD PTR [selection_half]
    fisub DWORD PTR [rcx+4]
    fld DWORD PTR [selection_half]
    fsub st(0),st(1)
    fmul DWORD PTR [selection_200]
    fadd DWORD PTR [rcx+12]
    fstp DWORD PTR [rdx+4]
    fmul DWORD PTR [selection_pi]
    fsin
    fmul DWORD PTR [selection_100]
    fld DWORD PTR [rcx+8]
    fsub DWORD PTR [selection_100]
    faddp st(1),st(0)
    fstp DWORD PTR [rdx]
    ret
sgn_selection_frame_xy ENDP
END
