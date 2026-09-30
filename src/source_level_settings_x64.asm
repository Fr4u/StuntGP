; New AMD64 ABI adaptation of42CCEC..42CCF5 and42CECC..42CEDD.
; ECX is the raw input word, EDX the borrowed live App46E2F0 bits,
; R8 points at the actual Session owner. No source binary bytes are copied.
; FXSAVE admission is non-mutating: all exceptions masked, PC24/53/64,
; empty stack, TOP0. No MXCSR change or substitute software FPU owner.
OPTION CASEMAP:NONE
.code

LEVEL_ADMISSION MACRO rejected
    fxsave [rsp+32]
    movzx eax,WORD PTR [rsp+32]
    and eax,03fh
    cmp eax,03fh
    jne rejected
    movzx eax,WORD PTR [rsp+32]
    and eax,0300h
    cmp eax,0100h
    je rejected
    cmp BYTE PTR [rsp+36],0
    jne rejected
    movzx eax,WORD PTR [rsp+34]
    and eax,03800h
    jnz rejected
ENDM

PUBLIC sgn_source_level_ground_42ccec
sgn_source_level_ground_42ccec PROC FRAME
    sub rsp,552
    .allocstack 552
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+8],edx
    LEVEL_ADMISSION ground_rejected
    fild DWORD PTR [rsp]          ;42CCEC signed DWORD
    fmul DWORD PTR [rsp+8]        ;42CCEF inherited PC/RC, App46E2F0
    fstp DWORD PTR [r8]           ;42CCF5 actual60DFA8 owner store
    xor eax,eax
    add rsp,552
    ret
ground_rejected:
    mov eax,1
    add rsp,552
    ret
sgn_source_level_ground_42ccec ENDP

PUBLIC sgn_source_level_rolling_42cecc
sgn_source_level_rolling_42cecc PROC FRAME
    sub rsp,552
    .allocstack 552
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],0        ;42CEC6/C9 zero-high caller qword
    mov DWORD PTR [rsp+8],edx
    LEVEL_ADMISSION rolling_rejected
    fild QWORD PTR [rsp]          ;42CECC signed64 with zero high DWORD
    fmul DWORD PTR [rsp+8]        ;42CECF inherited precision/rounding
    fstcw WORD PTR [rsp+12]       ;462866 waiting FSTCW
    fwait                        ;46286A additional WAIT
    mov ax,WORD PTR [rsp+12]
    or ah,0ch                    ;46286F only RC becomes truncate
    mov WORD PTR [rsp+14],ax
    fldcw WORD PTR [rsp+14]
    fistp QWORD PTR [rsp+16]      ;462879 signed64, including indefinite
    fldcw WORD PTR [rsp+12]       ;46287C restore this call's incoming CW
    mov eax,DWORD PTR [rsp+16]
    mov DWORD PTR [r8],eax        ;42CEDD stores low DWORD
    xor eax,eax
    add rsp,552
    ret
rolling_rejected:
    mov eax,1
    add rsp,552
    ret
sgn_source_level_rolling_42cecc ENDP
END
