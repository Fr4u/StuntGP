;43CE15..43CE39. Windows x64 leaf; RCX points to phase float and FTOL lowDWORD signed integer.
.const
widget_pi DWORD 040490FDBh
widget_amplitude DWORD 042200000h
widget_origin DWORD 043820000h
.code
PUBLIC sgn_menu_widget_y_43ce02
sgn_menu_widget_y_43ce02 PROC
    fld DWORD PTR [rcx]
    fisub DWORD PTR [rcx+4]
    fmul DWORD PTR [widget_pi]
    fsin
    fmul DWORD PTR [widget_amplitude]
    fsubr DWORD PTR [widget_origin]
    fstp DWORD PTR [rdx]
    ret
sgn_menu_widget_y_43ce02 ENDP
END
