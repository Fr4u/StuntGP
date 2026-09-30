option casemap:none
.code
; ECX contains the source signed DWORD bits. One FILD then one FSTP32;
; uses Win64 caller home space, changes no nonvolatile integer/control state.
sgn_menu_option_fild_fstp PROC
    mov DWORD PTR [rsp+8], ecx
    fild DWORD PTR [rsp+8]
    fstp DWORD PTR [rsp+8]
    mov eax, DWORD PTR [rsp+8]
    ret
sgn_menu_option_fild_fstp ENDP
END
