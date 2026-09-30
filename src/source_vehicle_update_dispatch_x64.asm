option casemap:none
.const
dispatch_zero DWORD 000000000h ;PE46E1EC
dispatch_half DWORD 03f000000h ;PE46E1D8
dispatch_sixty DWORD 042700000h ;PE46E1E0
dispatch_one DWORD 03f800000h ;PE46E264
dispatch_aux_force DWORD 03d088889h ;PE46E538
dispatch_aux_decay DWORD 03f666666h ;PE46E2E4
dispatch_height DWORD 0428c0000h ;PE46E53C
dispatch_ambience DWORD 044000000h ;PE46E5AC
.code

PUBLIC sgn_source_vehicle_clutch_40e470
;RCX actual shift_time, RDX actual clutch, R8 actual throttle.
;FST at40E4A1 keeps the extended value for comparison before any clamp.
sgn_source_vehicle_clutch_40e470 PROC
    fld DWORD PTR [rdx]
    fcomp DWORD PTR [dispatch_one]
    fnstsw ax
    test ah,1
    jz dispatch_clutch_done
    fld DWORD PTR [rcx]
    fmul DWORD PTR [dispatch_sixty]
    fdivr DWORD PTR [dispatch_one]
    fadd DWORD PTR [rdx]
    fst DWORD PTR [rdx]
    fcomp DWORD PTR [dispatch_one]
    fnstsw ax
    test ah,1
    jnz dispatch_clutch_low
    mov DWORD PTR [rdx],3f800000h
dispatch_clutch_low:
    fld DWORD PTR [rdx]
    fcomp DWORD PTR [dispatch_half]
    fnstsw ax
    test ah,1
    jz dispatch_clutch_done
    fld DWORD PTR [r8]
    fcomp DWORD PTR [dispatch_zero]
    fnstsw ax
    test ah,41h
    jnz dispatch_clutch_done
    mov DWORD PTR [r8],0
dispatch_clutch_done:
    ret
sgn_source_vehicle_clutch_40e470 ENDP

PUBLIC sgn_source_vehicle_wheel_speed_406270
;RCX actual speed, RDX four pointers to actual wheel.speed fields. No values
;are marshalled. The source speed remains in ST0 across all four stores.
sgn_source_vehicle_wheel_speed_406270 PROC
    fld DWORD PTR [rcx]
    mov r8d,4
dispatch_wheel_loop:
    mov r9,QWORD PTR [rdx]
    fld st(0)
    fsub DWORD PTR [r9]
    add rdx,8
    dec r8d
    fmul DWORD PTR [dispatch_half]
    fadd DWORD PTR [r9]
    fstp DWORD PTR [r9]
    jnz dispatch_wheel_loop
    fstp st(0)
    ret
sgn_source_vehicle_wheel_speed_406270 ENDP

PUBLIC sgn_source_vehicle_auxiliary_413330
;RCX actual2308 displacement, RDX actual2328 velocity; original four passes.
;X force stays extended; Y and Z force spill binary32. W is copied only to
;the original local and never written to either owner.
sgn_source_vehicle_auxiliary_413330 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r8d,4
dispatch_aux_loop:
    dec r8d
    mov eax,DWORD PTR [rcx]
    mov DWORD PTR [rsp],eax
    fld DWORD PTR [rsp]
    mov eax,DWORD PTR [rcx+4]
    fmul DWORD PTR [dispatch_aux_force]
    mov DWORD PTR [rsp+4],eax
    mov eax,DWORD PTR [rcx+8]
    fld DWORD PTR [rsp+4]
    fmul DWORD PTR [dispatch_aux_force]
    mov DWORD PTR [rsp+8],eax
    mov eax,DWORD PTR [rcx+12]
    mov DWORD PTR [rsp+12],eax
    fstp DWORD PTR [rsp+4]
    fld DWORD PTR [rsp+8]
    fmul DWORD PTR [dispatch_aux_force]
    fstp DWORD PTR [rsp+8]
    fld DWORD PTR [rdx]
    fsub st(0),st(1)
    fstp DWORD PTR [rdx]
    fstp st(0)
    fld DWORD PTR [rdx+4]
    fsub DWORD PTR [rsp+4]
    fstp DWORD PTR [rdx+4]
    fld DWORD PTR [rdx+8]
    fsub DWORD PTR [rsp+8]
    fstp DWORD PTR [rdx+8]
    fld DWORD PTR [rdx]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rdx+4]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx+8]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [dispatch_aux_decay]
    fstp DWORD PTR [rdx]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [dispatch_aux_decay]
    fstp DWORD PTR [rdx+4]
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [dispatch_aux_decay]
    fstp DWORD PTR [rdx+8]
    jnz dispatch_aux_loop
    add rsp,24
    ret
sgn_source_vehicle_auxiliary_413330 ENDP

PUBLIC sgn_source_vehicle_abs_compare
;Raw binary32 RCX, binary64 RDX. Return AH x87 comparison bits, including
;unordered. Float absolute value is compared directly with the source double.
sgn_source_vehicle_abs_compare PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov QWORD PTR [rsp+8],rdx
    fld DWORD PTR [rsp]
    fabs
    fcomp QWORD PTR [rsp+8]
    fnstsw ax
    movzx eax,ah
    add rsp,24
    ret
sgn_source_vehicle_abs_compare ENDP

PUBLIC sgn_source_vehicle_ambience_418150
;RCX actual DWORD7684, interpreted by FLD.418150 ->462860 preserves its
;unspilled product until FISTP signed64 with the original truncation CW.
sgn_source_vehicle_ambience_418150 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [dispatch_ambience]
    fstcw WORD PTR [rsp+16]
    wait
    mov ax,WORD PTR [rsp+16]
    or ah,0ch
    mov WORD PTR [rsp+18],ax
    fldcw WORD PTR [rsp+18]
    fistp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+16]
    mov eax,DWORD PTR [rsp]
    add rsp,24
    ret
sgn_source_vehicle_ambience_418150 ENDP

PUBLIC sgn_source_vehicle_pose_4181f5
;RCX the original80-byte local pose. RDX five pointers to actual world fields:
;position,up,basis_z,basis_y_w,contact.rig.origin_y_offset. No copied inputs.
;Template memcpy precedes this body. Source scale/tail/W stores remain literal.
sgn_source_vehicle_pose_4181f5 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r8,QWORD PTR [rdx]
    mov r9,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    fld DWORD PTR [dispatch_height]
    mov eax,DWORD PTR [r8]
    mov DWORD PTR [rcx],eax
    mov eax,DWORD PTR [r8+4]
    mov DWORD PTR [rcx+4],eax
    mov eax,DWORD PTR [r8+8]
    mov DWORD PTR [rcx+8],eax
    mov eax,DWORD PTR [r8+12]
    mov DWORD PTR [rcx+12],eax
    mov r11,QWORD PTR [rdx+32]
    fsub DWORD PTR [r11]
    fld st(0)
    fmul DWORD PTR [r9]
    fstp DWORD PTR [rsp]
    fld st(0)
    fmul DWORD PTR [r9+4]
    fstp DWORD PTR [rsp+4]
    fmul DWORD PTR [r9+8]
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rsp]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rcx+4]
    fsub DWORD PTR [rsp+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fsub st(0),st(1)
    fstp DWORD PTR [rcx+8]
    mov eax,DWORD PTR [r9]
    mov DWORD PTR [rcx+48],eax
    mov eax,DWORD PTR [r9+4]
    mov DWORD PTR [rcx+52],eax
    mov eax,DWORD PTR [r9+8]
    mov DWORD PTR [rcx+56],eax
    mov r11,QWORD PTR [rdx+24]
    mov eax,DWORD PTR [r11]
    mov DWORD PTR [rcx+60],eax
    mov eax,DWORD PTR [r10]
    fstp st(0)
    mov DWORD PTR [rcx+32],eax
    mov eax,DWORD PTR [r10+4]
    fld DWORD PTR [rcx+32]
    fadd DWORD PTR [rcx]
    mov DWORD PTR [rcx+36],eax
    mov eax,DWORD PTR [r10+8]
    mov DWORD PTR [rcx+40],eax
    mov eax,DWORD PTR [r10+12]
    fstp DWORD PTR [rcx+32]
    fld DWORD PTR [rcx+36]
    fadd DWORD PTR [rcx+4]
    mov DWORD PTR [rcx+44],eax
    fstp DWORD PTR [rcx+36]
    fld DWORD PTR [rcx+40]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+40]
    add rsp,24
    ret
sgn_source_vehicle_pose_4181f5 ENDP
END
