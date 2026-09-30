option casemap:none
.const
hud_46e1ec DWORD 000000000h ;PE0046E1EC
hud_46e2f0 DWORD 042c80000h ;PE0046E2F0
hud_46e294 DWORD 03c23d70ah ;PE0046E294
hud_46e3ec DWORD 043000000h ;PE0046E3EC
hud_46eb94 DWORD 0c3000000h ;PE0046EB94
hud_46e378 DWORD 043480000h ;PE0046E378
hud_46e2e0 DWORD 043800000h ;PE0046E2E0
hud_46e5fc DWORD 042000000h ;PE0046E5FC
hud_46eb4c DWORD 042bccccdh ;PE0046EB4C
hud_46eb48 DWORD 041600000h ;PE0046EB48
hud_46e20c DWORD 040800000h ;PE0046E20C
hud_46eb44 DWORD 042333333h ;PE0046EB44
hud_46eb40 DWORD 042a4cccdh ;PE0046EB40
hud_46e580 DWORD 03fc00000h ;PE0046E580
hud_46eb6c DWORD 040aaaaabh ;PE0046EB6C
hud_46eb68 DWORD 042840000h ;PE0046EB68
hud_46eb64 DWORD 0440f8000h ;PE0046EB64
hud_46eb70 DWORD 042accccch ;PE0046EB70
hud_46eb80 DWORD 042cccccdh ;PE0046EB80
hud_46e46c DWORD 042a00000h ;PE0046E46C
hud_46e754 DWORD 043f00000h ;PE0046E754
.code

; Source462860, preserving the caller's complete control word.
hud_ftol MACRO
    fnstcw WORD PTR [rsp+8]
    fwait
    mov ax,WORD PTR [rsp+8]
    or ah,0ch
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
    fistp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp]
ENDM

PUBLIC sgn_hud_layout_x87
sgn_hud_layout_x87 PROC
    ;RCX value, RDX countdown.value, R8 input, R9 output.447D86..447DC3,
    ;same numeric sequence repeated for the seven unrolled source records.
    fld DWORD PTR [rcx]
    fadd DWORD PTR [rdx]
    fcom DWORD PTR [hud_46e2f0]
    fnstsw ax
    test ah,41h
    jnz hud_layout_scaled
    fstp st(0)
    fld DWORD PTR [hud_46e2f0]
hud_layout_scaled:
    fild DWORD PTR [r8+8]
    fmul st(0),st(1)
    fmul DWORD PTR [hud_46e294]
    fadd DWORD PTR [r8]
    fstp DWORD PTR [r9+16]
    fild DWORD PTR [r8+12]
    fmul st(0),st(1)
    fmul DWORD PTR [hud_46e294]
    fadd DWORD PTR [r8+4]
    fstp DWORD PTR [r9+20]
    fstp st(0)
    ret
sgn_hud_layout_x87 ENDP

PUBLIC sgn_hud_displacement_x87
sgn_hud_displacement_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    ;44AC9F reads X,44ACA5 loads Y before either clamp/store.
    mov eax,DWORD PTR [rcx]
    fld DWORD PTR [rcx+4]
    mov DWORD PTR [rsp],eax
    fld DWORD PTR [rsp]
    fcomp DWORD PTR [hud_46e3ec]
    fnstsw ax
    test ah,41h
    jnz hud_disp_x_low
    mov DWORD PTR [rsp],043000000h
    jmp hud_disp_y_high
hud_disp_x_low:
    fld DWORD PTR [rsp]
    fcomp DWORD PTR [hud_46eb94]
    fnstsw ax
    test ah,1
    jz hud_disp_y_high
    mov DWORD PTR [rsp],0c3000000h
hud_disp_y_high:
    fcom DWORD PTR [hud_46e3ec]
    fnstsw ax
    test ah,41h
    jnz hud_disp_y_low
    fstp st(0)
    fld DWORD PTR [hud_46e3ec]
    jmp hud_disp_apply
hud_disp_y_low:
    fcom DWORD PTR [hud_46eb94]
    fnstsw ax
    test ah,1
    jz hud_disp_apply
    fstp st(0)
    fld DWORD PTR [hud_46eb94]
hud_disp_apply:
    mov eax,DWORD PTR [r8] ;44AD0A: selected HUD read AFTER both clamps
    cmp eax,4
    jae hud_disp_bad_owner
    imul rax,628 ;actual sizeof(RaceHudLayout), not a guest address token
    add rdx,rax
    mov r8d,6
hud_disp_pair:
    fld DWORD PTR [rsp]
    fadd DWORD PTR [rdx+16]
    fstp DWORD PTR [rdx+16]
    fld st(0)
    fadd DWORD PTR [rdx+20]
    fstp DWORD PTR [rdx+20]
    add rdx,24
    dec r8d
    jnz hud_disp_pair
    fld DWORD PTR [rsp]
    fadd DWORD PTR [rdx+16]
    fstp DWORD PTR [rdx+16]
    fadd DWORD PTR [rdx+20] ;last Y consumes the retained ST0,44ADA8
    fstp DWORD PTR [rdx+20]
    mov eax,1
    add rsp,16
    ret
hud_disp_bad_owner:
    fstp st(0) ;NEW HOST failure epilogue, no invented source store
    xor eax,eax
    add rsp,16
    ret
sgn_hud_displacement_x87 ENDP

PUBLIC sgn_hud_energy_ratio_x87
sgn_hud_energy_ratio_x87 PROC
    mov rdx,QWORD PTR [rcx]
    fld DWORD PTR [rdx]
    fcomp DWORD PTR [hud_46e1ec]
    fnstsw ax
    test ah,40h
    jnz hud_energy_zero
    fld DWORD PTR [rdx]
    mov r8,QWORD PTR [rcx+8]
    fdiv DWORD PTR [r8]
    mov r8,QWORD PTR [rcx+16]
    mov edx,DWORD PTR [r8] ;449413 captures the real selected word here
    mov r9,QWORD PTR [rcx+40]
    mov DWORD PTR [r9],edx ;lossless NEW invocation-local identity transport
    mov r8,QWORD PTR [rcx+32]
    fst DWORD PTR [r8] ;449419 FST, compare still uses unrounded ST0
    mov r8,QWORD PTR [rcx+24]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,1
    jnz hud_energy_low
    mov eax,2
    ret
hud_energy_low:
    mov eax,1
    ret
hud_energy_zero:
    xor eax,eax
    ret
sgn_hud_energy_ratio_x87 ENDP

PUBLIC sgn_hud_energy_phase_x87
sgn_hud_energy_phase_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [hud_46e378]
    hud_ftol
    add rsp,16
    ret
sgn_hud_energy_phase_x87 ENDP

PUBLIC sgn_hud_energy_volume_x87
sgn_hud_energy_volume_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rdx]
    fmul DWORD PTR [hud_46e2e0]
    fadd DWORD PTR [hud_46e5fc]
    hud_ftol
    add rsp,16
    ret
sgn_hud_energy_volume_x87 ENDP

PUBLIC sgn_hud_energy_draw_x87
sgn_hud_energy_draw_x87 PROC
    ;4494C3..4495CF. R8 points at the captured selected-HUD element0.
    fld DWORD PTR [rdx]
    fmul DWORD PTR [hud_46eb4c]
    mov DWORD PTR [rcx],6
    mov DWORD PTR [rcx+76],0
    mov DWORD PTR [rcx+80],0
    fld st(0)
    fadd DWORD PTR [hud_46eb48]
    mov DWORD PTR [rcx+84],0
    mov DWORD PTR [rcx+88],0442a0000h
    mov DWORD PTR [rcx+96],0
    mov DWORD PTR [rcx+104],0442a0000h
    mov DWORD PTR [rcx+112],041666667h
    mov DWORD PTR [rcx+120],044390000h
    fst DWORD PTR [rcx+92]
    fmul DWORD PTR [hud_46e20c]
    mov DWORD PTR [rcx+124],0
    mov DWORD PTR [rcx+128],041666667h
    mov DWORD PTR [rcx+132],0
    mov DWORD PTR [rcx+136],044390000h
    mov DWORD PTR [rcx+12],03f51eb85h
    mov DWORD PTR [rcx+16],042bccccdh
    fstp DWORD PTR [rcx+100]
    mov DWORD PTR [rcx+20],041666667h
    mov DWORD PTR [rcx+40],0bh
    fst DWORD PTR [rcx+108]
    fmul DWORD PTR [hud_46e20c]
    mov DWORD PTR [rcx+44],20h
    fstp DWORD PTR [rcx+116]
    fld DWORD PTR [r8+16]
    fadd DWORD PTR [hud_46eb44]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [r8+20]
    fadd DWORD PTR [hud_46eb40]
    fsub DWORD PTR [hud_46e580]
    fstp DWORD PTR [rcx+8]
    ret
sgn_hud_energy_draw_x87 ENDP

PUBLIC sgn_hud_notice_first_x87
sgn_hud_notice_first_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov eax,edx ;zero-extended ECX/ticks,449A1B..25
    mov QWORD PTR [rsp],rax
    fild QWORD PTR [rsp]
    mov DWORD PTR [rcx],2
    mov DWORD PTR [rcx+8],042933333h
    mov DWORD PTR [rcx+12],03f7ae148h
    fmul DWORD PTR [hud_46eb6c]
    mov DWORD PTR [rcx+24],0
    mov DWORD PTR [rcx+28],0
    mov DWORD PTR [rcx+32],044250000h
    mov DWORD PTR [rcx+36],043c00000h
    mov DWORD PTR [rcx+16],043040000h
    mov DWORD PTR [rcx+20],04299999ah
    fst DWORD PTR [r9] ;retain EXTENDED motion before FSUB,449A8D
    fsub DWORD PTR [hud_46eb68]
    mov DWORD PTR [rcx+40],0fh
    mov DWORD PTR [rcx+44],48h
    mov DWORD PTR [rcx+48],r8d
    fstp DWORD PTR [rcx+4]
    add rsp,16
    ret
sgn_hud_notice_first_x87 ENDP

PUBLIC sgn_hud_notice_second_x87
sgn_hud_notice_second_x87 PROC
    fld DWORD PTR [hud_46eb64]
    fsub DWORD PTR [rdx] ;reload the source FLOAT motion,449AC1
    mov DWORD PTR [rcx],2
    mov DWORD PTR [rcx+8],043166666h
    mov DWORD PTR [rcx+12],03f7ae148h
    mov DWORD PTR [rcx+24],0
    mov DWORD PTR [rcx+28],043c20000h
    fstp DWORD PTR [rcx+4]
    mov DWORD PTR [rcx+32],044500000h
    mov DWORD PTR [rcx+36],044210000h
    mov DWORD PTR [rcx+16],043266667h
    mov DWORD PTR [rcx+20],0424ccccdh
    mov DWORD PTR [rcx+40],0fh
    mov DWORD PTR [rcx+44],48h
    mov DWORD PTR [rcx+48],r8d
    ret
sgn_hud_notice_second_x87 ENDP

PUBLIC sgn_hud_finish_x87
sgn_hud_finish_x87 PROC
    fld DWORD PTR [hud_46eb70]
    fsub DWORD PTR [rdx]
    mov DWORD PTR [rcx],2
    mov DWORD PTR [rcx+4],043266666h
    mov DWORD PTR [rcx+12],03f7d70a4h
    mov DWORD PTR [rcx+24],0
    mov DWORD PTR [rcx+28],0
    mov DWORD PTR [rcx+32],044800000h
    fstp DWORD PTR [rcx+8]
    mov DWORD PTR [rcx+36],044800000h
    mov DWORD PTR [rcx+16],04399999ah
    mov DWORD PTR [rcx+20],04399999ah
    mov DWORD PTR [rcx+40],0fh
    mov DWORD PTR [rcx+44],48h
    mov DWORD PTR [rcx+48],r8d
    ret
sgn_hud_finish_x87 ENDP

PUBLIC sgn_hud_start_x87
sgn_hud_start_x87 PROC
    fld DWORD PTR [rdx]
    fadd DWORD PTR [hud_46eb80]
    mov DWORD PTR [rcx],2
    mov DWORD PTR [rcx+4],043266666h
    mov DWORD PTR [rcx+12],03f7d70a4h
    mov DWORD PTR [rcx+24],0
    mov DWORD PTR [rcx+28],0
    mov DWORD PTR [rcx+32],044800000h
    fstp DWORD PTR [rcx+8]
    mov DWORD PTR [rcx+36],044800000h
    mov DWORD PTR [rcx+16],04399999ah
    mov DWORD PTR [rcx+20],04319999ah
    mov DWORD PTR [rcx+40],0fh
    mov DWORD PTR [rcx+44],48h
    mov DWORD PTR [rcx+48],r8d
    ret
sgn_hud_start_x87 ENDP

PUBLIC sgn_hud_wrong_y_x87
sgn_hud_wrong_y_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx]
    fadd DWORD PTR [hud_46e46c]
    fstp DWORD PTR [rsp]
    mov eax,DWORD PTR [rsp]
    add rsp,16
    ret
sgn_hud_wrong_y_x87 ENDP

PUBLIC sgn_hud_notice_y_x87
sgn_hud_notice_y_x87 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov eax,ecx
    mov QWORD PTR [rsp],rax ;442EA5/EA8, unsigned animation zero-extended
    fild QWORD PTR [rsp]
    fsubr DWORD PTR [hud_46e754]
    fstp DWORD PTR [rsp]
    mov eax,DWORD PTR [rsp]
    add rsp,16
    ret
sgn_hud_notice_y_x87 ENDP
END
