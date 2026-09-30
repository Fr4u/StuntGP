option casemap:none
.code
PUBLIC sgn_scene_crt_power_462bb0
sgn_scene_crt_power_462bb0 PROC FRAME
    sub rsp,72
    .allocstack 72
    .endprolog
    mov r10,rcx
    mov r11,QWORD PTR [r10+8]
    mov rdx,QWORD PTR [r10]
    ;NEW Win64 transport, before source462BB0. Preserve a foreign stack on
    ;rejection; only an empty physical stack admits reconstruction of TOP6.
    fnstenv [rsp]
    cmp WORD PTR [rsp+8],0ffffh
    jne power_bad_transport
    mov ax,WORD PTR [rdx+22]
    and ax,03800h
    cmp ax,03000h
    jne power_bad_transport
    fninit
    fld TBYTE PTR [rdx+10]
    fld TBYTE PTR [rdx]
    fnstenv [rsp+32]
    mov ax,WORD PTR [rdx+20]
    mov WORD PTR [rsp+32],ax
    mov ax,WORD PTR [rdx+22]
    mov WORD PTR [rsp+36],ax
    fldenv [rsp+32]

    ;462BB0: exponent stays extended in ST0 after the two source DOUBLE
    ;stores. Computation must reload only the rounded base at466284.
    fxch st(1)                  ;462BB3
    fstp QWORD PTR [r10+48]      ;462BB5
    fst QWORD PTR [r10+56]       ;462BB8, non-popping
    mov eax,DWORD PTR [r10+60]  ;462BBC, exponent high word
    mov ecx,eax                 ;462BD2
    fstcw WORD PTR [r10+36]      ;462BD5, waiting source instruction
    cmp WORD PTR [r10+36],027fh
    je power_classify_exponent
    ;466245: clear RC, set all exception masks, preserve actual PC bits.
    movzx edx,WORD PTR [r10+36]
    and edx,0300h
    or edx,07fh
    mov WORD PTR [r10+38],dx
    fldcw WORD PTR [r10+38]
power_classify_exponent:
    and ecx,07ff00000h          ;462BE6
    cmp ecx,07ff00000h
    je power_special_exponent  ;462BF6 ->462C99, before loading base

    ;466275's finite branch. Its CMP flags are not confused with FLD flags:
    ;462C01 JZ is false here; special dispatch stops at actual466282.
    mov eax,DWORD PTR [r10+52]
    and eax,07ff00000h
    cmp eax,07ff00000h
    je power_special_base
    fld QWORD PTR [r10+48]       ;466284
    test eax,07ff00000h          ;462C07
    jz power_zero_or_denormal
power_base_sign:
    mov cl,BYTE PTR [r10+55]    ;462C12
    and cl,080h
    jnz power_negative_base
    fyl2x                       ;462C1F
    ;Complete466230. Do not use exp2, log2 or integer multiplication.
    fld st(0)
    frndint
    fsubr st(1),st(0)
    fxch st(1)
    fchs
    f2xm1
    fld1
    faddp st(1),st(0)
    fscale
    fstp st(1)
    cmp cl,1                    ;462C26; positive branch CL is exactly0
    jne power_postprocess
    fchs
power_postprocess:
    cmp DWORD PTR [r11],0       ;462C2D, actual process owner reread each call
    jne power_restore          ;462C34 ->4662CE
    ;ECX=4959B8 name and EDX=1D are retained below if error handling is reached.
    fst QWORD PTR [r10+64]       ;46631C, actual local FLOAT64; keep ST0 extended
    mov eax,DWORD PTR [r10+68]
    and eax,07ff00000h
    mov DWORD PTR [r10+40],eax  ;NEW diagnostic after the source classification
    jz power_underflow          ;46632B
    cmp eax,07ff00000h
    je power_overflow           ;466332
    mov ax,WORD PTR [r10+36]
    cmp ax,027fh
    je power_return             ;46633C ->466368, no redundant FLDCW
    and ax,020h
    jnz power_restore           ;466342
    fstsw ax                    ;466344
    and ax,020h
    jz power_restore            ;46634B
    ;46634D code8, EDX==1D,46635E ->4661D0. Build the actual initialized
    ;exception record, including original rounded args and source FSTP retval.
    mov DWORD PTR [r10+72],8
    mov eax,DWORD PTR [r10+56]
    mov DWORD PTR [r10+88],eax  ;4661D9/DC copy exponent into arg2
    mov eax,DWORD PTR [r10+60]
    mov DWORD PTR [r10+92],eax  ;4661DF/E2
    fstp QWORD PTR [r10+96]      ;4661F0
    mov DWORD PTR [r10+76],04959b8h ;4661F3
    mov eax,DWORD PTR [r10+48]
    mov edx,DWORD PTR [r10+52]
    mov DWORD PTR [r10+80],eax
    mov DWORD PTR [r10+84],edx
    mov DWORD PTR [r10+28],046620bh
    mov DWORD PTR [r10+32],046b6d5h
    mov eax,4
    jmp power_host_unwind

power_zero_or_denormal:
    mov eax,DWORD PTR [r10+52]  ;462D08
    and eax,0fffffh
    or eax,DWORD PTR [r10+48]
    jnz power_base_sign         ;462D15, nonzero denormal reaches same body
    mov DWORD PTR [r10+28],0462d15h
    mov DWORD PTR [r10+32],0462d1bh
    mov eax,2
    jmp power_host_unwind
power_restore:
    cmp WORD PTR [r10+36],027fh ;4662CE /466365 equivalent restoration
    je power_return
    fldcw WORD PTR [r10+36]
power_return:
    ;NEW binary80 return transport. Retain source condition/sticky bits too:
    ;FSTP TBYTE can change C1, so restore captured SW with the drained TOP.
    fnstsw WORD PTR [r10+26]
    fstp TBYTE PTR [r10+16]
    fnstenv [rsp+32]
    mov ax,WORD PTR [r10+26]
    and ax,0c7ffh              ;remove only source TOP7
    mov dx,WORD PTR [rsp+36]
    and dx,03800h             ;physical drained TOP0
    or ax,dx
    mov WORD PTR [rsp+36],ax
    fldenv [rsp+32]
    xor eax,eax
    add rsp,72
    ret

power_special_exponent:
    mov DWORD PTR [r10+28],0462bf6h
    mov DWORD PTR [r10+32],0462c99h
    mov eax,2
    jmp power_host_unwind
power_special_base:
    mov DWORD PTR [r10+28],0466282h
    mov DWORD PTR [r10+32],0466287h
    mov eax,2
    jmp power_host_unwind
power_negative_base:
    mov DWORD PTR [r10+28],0462c19h
    mov DWORD PTR [r10+32],0462d80h
    mov eax,2
    jmp power_host_unwind
power_underflow:
    mov DWORD PTR [r10+28],046632bh
    mov DWORD PTR [r10+32],046636ah
    mov eax,3
    jmp power_host_unwind
power_overflow:
    mov DWORD PTR [r10+28],0466332h
    mov DWORD PTR [r10+32],0466393h
    mov eax,3
    jmp power_host_unwind
power_bad_transport:
    mov DWORD PTR [r10+28],0462bb0h
    mov DWORD PTR [r10+32],0462bd2h
    mov eax,1
power_host_unwind:
    ;NEW failed-call unwind only: source argument/result/exception diagnostics
    ;remain. No CRT errno/matherr success is invented. Foreign entry stack
    ;was never modified on the transport-rejection branch.
    fnstsw WORD PTR [r10+26]
    fldenv [rsp]
    add rsp,72
    ret
sgn_scene_crt_power_462bb0 ENDP
END
