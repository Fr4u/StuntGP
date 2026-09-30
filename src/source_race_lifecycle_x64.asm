option casemap:none
; Win64 ABI numeric leaves; actual source x87 operation/store/compare order.
; CW is inherited from429510. FTOL alone changes RC to truncate then restores it.
.const
lifecycle_half DWORD 03f000000h ;46E1D8
lifecycle_turn DWORD 043b40000h ;46E1E8
lifecycle_damping DWORD 03f7ae148h ;46E730
lifecycle_four DWORD 040800000h ;46E20C
lifecycle_ten DWORD 041200000h ;46E220
lifecycle_quarter DWORD 03e800000h ;46E24C
lifecycle_one DWORD 03f800000h ;46E264
lifecycle_accel DWORD 0bc23d70ah ;46E654
lifecycle_drag DWORD 03f733333h ;46E234
lifecycle_min_speed DWORD 042055555h ;46E778
lifecycle_max_speed DWORD 042c80000h ;46E2F0
lifecycle_percent DWORD 03c23d70ah ;46E294
lifecycle_rolling_margin DWORD 046ea6000h ;46E4EC
.code

;40591D..40592F: input DWORD reinterpreted signed by FILD; no SSE int conversion.
PUBLIC sgn_lifecycle_i32_float
sgn_lifecycle_i32_float PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],ecx
    fild DWORD PTR [rsp]
    fstp DWORD PTR [rsp+4]
    movss xmm0,DWORD PTR [rsp+4]
    add rsp,24
    ret
sgn_lifecycle_i32_float ENDP

;4437C8..4437D3 with complete462860 including integer-indefinite low DWORD.
PUBLIC sgn_lifecycle_float_low
sgn_lifecycle_float_low PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ax,0c00h
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
    add rsp,24
    ret
sgn_lifecycle_float_low ENDP

;443A43..443A68. C0 alone: unordered takes the rolling-start arm.
PUBLIC sgn_lifecycle_rolling
sgn_lifecycle_rolling PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fmul DWORD PTR [lifecycle_percent]
    fld DWORD PTR [rcx]
    fsub DWORD PTR [lifecycle_rolling_margin]
    fcompp
    fnstsw ax
    shr eax,8
    and eax,1
    ret
sgn_lifecycle_rolling ENDP

;4445EE..444642. RDX points to actual f22; adjacent f23 is cleared between stores.
PUBLIC sgn_lifecycle_finish_speed
sgn_lifecycle_finish_speed PROC
    fld DWORD PTR [rcx]
    fabs
    fstp DWORD PTR [rdx]
    mov DWORD PTR [rdx+4],0
    fld DWORD PTR [rdx]
    fcomp DWORD PTR [lifecycle_min_speed]
    fnstsw ax
    test ah,1
    jz finish_speed_upper
    mov DWORD PTR [rdx],042055555h
    ret
finish_speed_upper:
    fld DWORD PTR [rdx]
    fcomp DWORD PTR [lifecycle_max_speed]
    fnstsw ax
    test ah,41h
    jnz finish_speed_done
    mov DWORD PTR [rdx],042c80000h
finish_speed_done:
    ret
sgn_lifecycle_finish_speed ENDP

;444B68..444BB2 and444497..4444E2 share the exact three predicates/stores.
;RCX points to actual contiguous position,velocity,acceleration DWORDs.
PUBLIC sgn_lifecycle_settle
sgn_lifecycle_settle PROC
    fld DWORD PTR [rcx+8]
    fabs
    fcomp DWORD PTR [lifecycle_quarter]
    fnstsw ax
    test ah,1
    jz settle_move
    fld DWORD PTR [rcx+4]
    fabs
    fcomp DWORD PTR [lifecycle_quarter]
    fnstsw ax
    test ah,1
    jz settle_move
    fld DWORD PTR [rcx]
    fabs
    fcomp DWORD PTR [lifecycle_one]
    fnstsw ax
    test ah,1
    jz settle_move
    mov DWORD PTR [rcx+8],0
    mov DWORD PTR [rcx+4],0
    mov DWORD PTR [rcx],0
    mov eax,1
    ret
settle_move:
    ;444BDD..444BF8 /444514..44453E retain extended intermediates after FST.
    fld DWORD PTR [rcx]
    fmul DWORD PTR [lifecycle_accel]
    fst DWORD PTR [rcx+8]
    fadd DWORD PTR [rcx+4]
    fmul DWORD PTR [lifecycle_drag]
    fst DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    xor eax,eax
    ret
sgn_lifecycle_settle ENDP

;4447CB..444873. RCX points to actual f21(angle), f22(speed), f23(stop flag).
;Native-only return low byte carries source ESI sound; bit8 reports settled arm.
PUBLIC sgn_lifecycle_spinner
sgn_lifecycle_spinner PROC
    xor r10d,r10d
    xor r11d,r11d
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [lifecycle_half]
    fadd DWORD PTR [rcx]
    fcom DWORD PTR [lifecycle_turn]
    fst DWORD PTR [rcx]
    fnstsw ax
    test ah,1
    jnz spinner_angle_pop
    fsub DWORD PTR [lifecycle_turn]
    mov r10d,46
    fstp DWORD PTR [rcx]
    jmp spinner_speed
spinner_angle_pop:
    fstp st(0)
spinner_speed:
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [lifecycle_damping]
    fst DWORD PTR [rcx+4]
    fcomp DWORD PTR [lifecycle_four]
    fnstsw ax
    test ah,1
    jz spinner_check_speed
    mov DWORD PTR [rcx+4],040800000h
spinner_check_speed:
    fld DWORD PTR [rcx+4]
    fcomp DWORD PTR [lifecycle_four]
    fnstsw ax
    test ah,41h
    jz spinner_done
    fld DWORD PTR [rcx]
    fcomp DWORD PTR [lifecycle_ten]
    fnstsw ax
    test ah,41h
    jz spinner_done
    cmp DWORD PTR [rcx+8],0
    jne spinner_stopped
    mov DWORD PTR [rcx+8],1
    mov r10d,50
spinner_stopped:
    mov DWORD PTR [rcx],0
    mov DWORD PTR [rcx+4],0
    mov r11d,100h
spinner_done:
    mov eax,r10d
    or eax,r11d
    ret
sgn_lifecycle_spinner ENDP

;401280..401289: source unsigned DWORD is converted by zero-extended FILD64.
PUBLIC sgn_score_factory_add
sgn_score_factory_add PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],edx
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    add rsp,24
    ret
sgn_score_factory_add ENDP

;4016F4..401754 with the original FST-preserved width and height intermediates.
PUBLIC sgn_score_factory_layout
sgn_score_factory_layout PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx+0bch]
    fmul DWORD PTR [lifecycle_half]
    fstp DWORD PTR [rsp]
    fld DWORD PTR [rcx+0c0h]
    fmul DWORD PTR [lifecycle_half]
    cmp edx,0
    je factory_full_width
    cmp edx,1
    je factory_full_width
    cmp edx,2
    jne factory_half_width
factory_full_width:
    fld DWORD PTR [rcx+0bch]
    jmp factory_store_layout
factory_half_width:
    fld DWORD PTR [rsp]
factory_store_layout:
    fst DWORD PTR [rcx+0e4h]
    fld st(1)
    fstp DWORD PTR [rcx+0e8h]
    mov DWORD PTR [rsp+4],043a00000h
    fld DWORD PTR [rsp+4]
    fsub st(0),st(1)
    fstp DWORD PTR [rcx+0b0h]
    fstp st(0)
    mov DWORD PTR [rsp+4],043700000h
    fld DWORD PTR [rsp+4]
    fsub st(0),st(1)
    fstp DWORD PTR [rcx+0b4h]
    fstp st(0)
    add rsp,24
    ret
sgn_score_factory_layout ENDP
;447AB0 with direct pointers to the existing split HUD owner fields.
;RCX=value, RDX=active, R8=phase. FST retains extended next for the comparison.
PUBLIC sgn_lifecycle_results_hud
sgn_lifecycle_results_hud PROC
    cmp DWORD PTR [rdx],0
    je results_hud_return
    fld DWORD PTR [rcx]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,1
    jz results_hud_down
    fld DWORD PTR [r8]
    fsub DWORD PTR [rcx]
    fsubr DWORD PTR [results_hud_hundred]
    fmul DWORD PTR [results_hud_sixteenth]
    fcom DWORD PTR [results_hud_minimum]
    fnstsw ax
    test ah,1
    jz results_hud_add
    fstp st(0)
    fld DWORD PTR [results_hud_minimum]
results_hud_add:
    fadd DWORD PTR [rcx]
    fst DWORD PTR [rcx]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,41h
    jnz results_hud_return
    jmp results_hud_end
results_hud_down:
    fld DWORD PTR [rcx]
    fsub DWORD PTR [r8]
    fmul DWORD PTR [results_hud_sixteenth]
    fcom DWORD PTR [results_hud_minimum]
    fnstsw ax
    test ah,1
    jz results_hud_subtract
    fstp st(0)
    fld DWORD PTR [results_hud_minimum]
results_hud_subtract:
    fsubr DWORD PTR [rcx]
    fst DWORD PTR [rcx]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,1
    jz results_hud_return
results_hud_end:
    mov eax,DWORD PTR [r8]
    mov DWORD PTR [rdx],0
    mov DWORD PTR [rcx],eax
results_hud_return:
    ret
sgn_lifecycle_results_hud ENDP
PUBLIC sgn_lifecycle_all_overlay_rate
sgn_lifecycle_all_overlay_rate PROC
    fld QWORD PTR [rcx]
    fcom DWORD PTR [lifecycle_one]
    fnstsw ax
    test ah,1
    jz all_overlay_rate_store
    fsub DWORD PTR [all_overlay_subtract]
all_overlay_rate_store:
    fst DWORD PTR [rdx]
    fstp QWORD PTR [rcx]
    ret
sgn_lifecycle_all_overlay_rate ENDP
;4431EB unsigned offset and FSUBR then source binary32 argument store.
PUBLIC sgn_trial_distance
sgn_trial_distance PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    fsubr DWORD PTR [rdx]
    fstp DWORD PTR [rsp+8]
    movss xmm0,DWORD PTR [rsp+8]
    add rsp,24
    ret
sgn_trial_distance ENDP

;4289B4..4289C7: FST spills parameter but462860 still sees unrounded ST0.
PUBLIC sgn_trial_quotient
sgn_trial_quotient PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    fmul DWORD PTR [rdx]
    fdiv DWORD PTR [r8]
    fst DWORD PTR [r9]
    fstcw WORD PTR [rsp+16]
    fwait
    mov ax,WORD PTR [rsp+16]
    or ax,0c00h
    mov WORD PTR [rsp+18],ax
    fldcw WORD PTR [rsp+18]
    fistp QWORD PTR [rsp+24]
    fldcw WORD PTR [rsp+16]
    mov eax,DWORD PTR [rsp+24]
    add rsp,40
    ret
sgn_trial_quotient ENDP

;418D46..418D55: unsigned descriptor-adjacent DWORD, two FDIVR,462860 low word.
PUBLIC sgn_trial_special_offset
sgn_trial_special_offset PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    fdivr DWORD PTR [rdx]
    fdivr DWORD PTR [trial_five_thousand]
    fstcw WORD PTR [rsp+16]
    fwait
    mov ax,WORD PTR [rsp+16]
    or ax,0c00h
    mov WORD PTR [rsp+18],ax
    fldcw WORD PTR [rsp+18]
    fistp QWORD PTR [rsp+24]
    fldcw WORD PTR [rsp+16]
    mov eax,DWORD PTR [rsp+24]
    add rsp,40
    ret
sgn_trial_special_offset ENDP

;Literal44ED40 stack arithmetic and C3 branch, including unordered zeroing.
PUBLIC sgn_trial_normalize
sgn_trial_normalize PROC
    fld DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+4]
    fld DWORD PTR [rcx]
    fld st(0)
    fmul st(0),st(1)
    fld st(2)
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp st(3)
    fstp st(0)
    fstp st(0)
    fcom DWORD PTR [trial_zero]
    fnstsw ax
    test ah,40h
    jz trial_normalize_nonzero
    fstp st(0)
    mov DWORD PTR [rcx+8],0
    mov DWORD PTR [rcx+4],0
    mov DWORD PTR [rcx],0
    ret
trial_normalize_nonzero:
    fsqrt
    fdivr DWORD PTR [lifecycle_one]
    fld st(0)
    fmul DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld st(0)
    fmul DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fmul DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
    ret
sgn_trial_normalize ENDP
.const
trial_five_thousand DWORD 0459c4000h
trial_zero DWORD 0
all_overlay_subtract DWORD 03d4ccccdh
results_hud_hundred DWORD 042c80000h
results_hud_sixteenth DWORD 03d800000h
results_hud_minimum DWORD 03dcccccDh
END
