OPTION CASEMAP:NONE
; Full43B470 arithmetic/store order, using the existing inherited x87 CW.
; RCX points to three actual signed bytes in43BBB0's output local. No calls,
; nonvolatile register use, FNINIT/FNCLEX, SSE substitute, or zero-vector fallback.
.const
replay_127_46e760 QWORD 0405fc00000000000h
.code
PUBLIC sgn_replay_normalize_43b470
sgn_replay_normalize_43b470 PROC FRAME
    sub rsp,38h
    .allocstack 38h
    .endprolog
    mov r10,rcx
    movsx eax,BYTE PTR [r10]
    movsx ecx,BYTE PTR [r10+1]
    movsx edx,BYTE PTR [r10+2]
    mov DWORD PTR [rsp],eax
    mov DWORD PTR [rsp+8],edx
    fild DWORD PTR [rsp]
    mov DWORD PTR [rsp],ecx
    fild DWORD PTR [rsp]
    fstp DWORD PTR [rsp+4]
    fild DWORD PTR [rsp+8]
    fst DWORD PTR [rsp]
    fmul DWORD PTR [rsp]
    fld DWORD PTR [rsp+4]
    fmul DWORD PTR [rsp+4]
    faddp st(1),st(0)
    fld st(1)
    fmul st(0),st(2)
    faddp st(1),st(0)
    fsqrt
    fdivr QWORD PTR [replay_127_46e760]
    fst DWORD PTR [rsp+8]
    fmul st(0),st(1)
    ; Inline462860: signed64 conversion under temporary truncation CW.
    fstcw WORD PTR [rsp+12]
    fwait
    mov ax,WORD PTR [rsp+12]
    or ah,0ch
    mov WORD PTR [rsp+14],ax
    fldcw WORD PTR [rsp+14]
    fistp QWORD PTR [rsp+16]
    fldcw WORD PTR [rsp+12]
    mov r8d,DWORD PTR [rsp+16]
    fstp st(0)
    fld DWORD PTR [rsp+8]
    fmul DWORD PTR [rsp+4]
    mov BYTE PTR [r10],r8b
    fstcw WORD PTR [rsp+12]
    fwait
    mov ax,WORD PTR [rsp+12]
    or ah,0ch
    mov WORD PTR [rsp+14],ax
    fldcw WORD PTR [rsp+14]
    fistp QWORD PTR [rsp+16]
    fldcw WORD PTR [rsp+12]
    mov r8d,DWORD PTR [rsp+16]
    fld DWORD PTR [rsp+8]
    fmul DWORD PTR [rsp]
    mov BYTE PTR [r10+1],r8b
    fstcw WORD PTR [rsp+12]
    fwait
    mov ax,WORD PTR [rsp+12]
    or ah,0ch
    mov WORD PTR [rsp+14],ax
    fldcw WORD PTR [rsp+14]
    fistp QWORD PTR [rsp+16]
    fldcw WORD PTR [rsp+12]
    mov eax,DWORD PTR [rsp+16]
    mov BYTE PTR [r10+2],al
    add rsp,38h
    ret
sgn_replay_normalize_43b470 ENDP
END
