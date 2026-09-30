option casemap:none
.const
light_one DWORD 03f800000h
light_radius DWORD 043fa0000h ;PE46E22C,500
light_far_radius DWORD 0459c4000h ;PE46E290,5000
light_scale DWORD 0437f0000h ;PE46E1E4,255
light_half_scale DWORD 0443f4000h ;PE46E6F4,765
light_half DWORD 03f000000h ;PE46E1D8,.5
light_intensity DWORD 04a7f0000h ;PE46E6F0,4177920
fade_delta DWORD 03e4ccccdh
fade_zero DWORD 000000000h
fade_current_down DWORD 0bd4ccccdh
fade_current_threshold DWORD 040400000h
fade_blend_up DWORD 03bf5c28fh
fade_blend_down DWORD 03d4ccccdh
.code
source_model_ftol MACRO
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ax,0c00h
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
ENDM
light_dot MACRO
    fld DWORD PTR [r10+28h]
    fmul DWORD PTR [rcx+8]
    fld DWORD PTR [r10+24h]
    fmul DWORD PTR [rcx+4]
    faddp st(1),st(0)
    fld DWORD PTR [r10+20h]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
ENDM
PUBLIC sgn_source_light_visibility_42d9e7
;rcx actual light144;rdx pointer bundle: cameraXYZ,forward,planes,far,near.
;Caller has already taken active!=0 and kind0/1. Relative W is source-stack
;undefined in42D9B0: retain destination W, do not fabricate an input DWORD.
sgn_source_light_visibility_42d9e7 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r10,rcx
    mov r11,rdx
    mov r8,QWORD PTR [r11]
    fld DWORD PTR [r10+10h]
    fsub DWORD PTR [r8]
    fstp DWORD PTR [rsp]
    fld DWORD PTR [r10+14h]
    fsub DWORD PTR [r8+4]
    mov eax,DWORD PTR [rsp]
    mov DWORD PTR [r10+20h],eax
    fstp DWORD PTR [rsp+4]
    fld DWORD PTR [r10+18h]
    fsub DWORD PTR [r8+8]
    mov eax,DWORD PTR [rsp+4]
    mov DWORD PTR [r10+24h],eax
    fstp DWORD PTR [rsp+8]
    cmp DWORD PTR [r10+4],0
    je directional_visibility
    fld DWORD PTR [r10+8]
    fcomp DWORD PTR [light_one]
    mov eax,DWORD PTR [rsp+8]
    mov DWORD PTR [r10+28h],eax
    fnstsw ax
    test ah,1
    jz point_strength
    fld DWORD PTR [light_one]
    jmp point_radius
point_strength:
    fld DWORD PTR [r10+8]
point_radius:
    fmul DWORD PTR [light_radius]
    fstp DWORD PTR [rsp+12]
    mov rcx,QWORD PTR [r11+8]
    light_dot
    mov r8,QWORD PTR [r11+24]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,41h
    jz point_rejected
    mov rcx,QWORD PTR [r11+16]
    mov r9d,4
point_planes:
    light_dot
    fcomp DWORD PTR [rsp+12]
    fnstsw ax
    test ah,41h
    jz point_rejected
    add rcx,16
    dec r9d
    jnz point_planes
    mov eax,1
    jmp point_visible_store
point_rejected:
    xor eax,eax
point_visible_store:
    xor r8d,r8d
    cmp DWORD PTR [r10+50h],3
    cmove r8d,eax
    mov DWORD PTR [r10+84h],r8d
    jmp visibility_done
directional_visibility:
    mov eax,DWORD PTR [rsp+8]
    mov DWORD PTR [r10+28h],eax
    cmp DWORD PTR [r10+50h],3
    jne directional_rejected
    mov rcx,QWORD PTR [r11+8]
    light_dot
    mov r8,QWORD PTR [r11+24]
    fcom DWORD PTR [r8]
    fnstsw ax
    test ah,41h
    jnz directional_near
    fstp st(0)
    jmp directional_rejected
directional_near:
    mov r8,QWORD PTR [r11+32]
    fcomp DWORD PTR [r8]
    fnstsw ax
    test ah,1
    jnz directional_rejected
    mov rcx,QWORD PTR [r11+16]
    mov r9d,4
directional_planes:
    light_dot
    fcomp DWORD PTR [light_far_radius]
    fnstsw ax
    test ah,41h
    jz directional_rejected
    add rcx,16
    dec r9d
    jnz directional_planes
    mov DWORD PTR [r10+84h],1
    jmp directional_included
directional_rejected:
    mov DWORD PTR [r10+84h],0
directional_included:
    mov eax,1
visibility_done:
    add rsp,24
    ret
sgn_source_light_visibility_42d9e7 ENDP

PUBLIC sgn_source_light_rotation_44fb00
;six pinned samples in source FLD order:cx,cy,cz,sx,sy,sz. rdx=actual61C3A4.
sgn_source_light_rotation_44fb00 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx]
    fld DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+12]
    fld DWORD PTR [rcx+16]
    fld DWORD PTR [rcx+20]
    xor eax,eax
    FSTP DWORD PTR [rsp+16] ;0044fb43
    FLD st(2) ;0044fb46
    FMUL st(0),st(4) ;0044fb48
    MOV DWORD PTR [rdx + 0ch],eax ;0044fb4a
    MOV DWORD PTR [rdx + 01ch],eax ;0044fb4d
    MOV DWORD PTR [rdx + 02ch],eax ;0044fb50
    MOV DWORD PTR [rdx + 030h],eax ;0044fb53
    MOV DWORD PTR [rdx + 034h],eax ;0044fb56
    MOV DWORD PTR [rdx + 038h],eax ;0044fb59
    FSTP DWORD PTR [rdx] ;0044fb5c
    FLD DWORD PTR [rsp+16] ;0044fb5e
    FMUL st(0),st(4) ;0044fb61
    MOV DWORD PTR [rdx + 03ch],03f800000h ;0044fb63
    FSTP DWORD PTR [rdx + 04h] ;0044fb6b
    FLD st(0) ;0044fb6e
    FCHS ;0044fb70
    FSTP DWORD PTR [rdx + 08h] ;0044fb72
    FLD st(0) ;0044fb75
    FMUL st(0),st(2) ;0044fb77
    FLD st(0) ;0044fb79
    FMUL st(0),st(4) ;0044fb7b
    FLD DWORD PTR [rsp+16] ;0044fb7d
    FMUL st(0),st(7) ;0044fb80
    fsubp st(1),st(0) ;0044fb82
    FSTP DWORD PTR [rdx + 010h] ;0044fb84
    FMUL DWORD PTR [rsp+16] ;0044fb87
    FLD st(3) ;0044fb8a
    FMUL st(0),st(6) ;0044fb8c
    faddp st(1),st(0) ;0044fb8e
    FSTP DWORD PTR [rdx + 014h] ;0044fb90
    FLD st(1) ;0044fb93
    FMUL st(0),st(4) ;0044fb95
    FSTP DWORD PTR [rdx + 018h] ;0044fb97
    FMUL st(0),st(4) ;0044fb9a
    FLD DWORD PTR [rsp+16] ;0044fb9c
    FMUL st(0),st(2) ;0044fb9f
    FLD st(1) ;0044fba1
    FMUL st(0),st(4) ;0044fba3
    faddp st(1),st(0) ;0044fba5
    FSTP DWORD PTR [rdx + 020h] ;0044fba7
    FMUL DWORD PTR [rsp+16] ;0044fbaa
    fxch st(1) ;0044fbad
    FMUL st(0),st(2) ;0044fbaf
    fsubp st(1),st(0) ;0044fbb1
    FSTP DWORD PTR [rdx + 024h] ;0044fbb3
    FSTP st(0) ;0044fbb6
    FMUL st(0),st(1) ;0044fbb8
    FSTP DWORD PTR [rdx + 028h] ;0044fbba
    FSTP st(0) ;0044fbbd
    add rsp,24
    ret
sgn_source_light_rotation_44fb00 ENDP
PUBLIC sgn_source_light_rotate_44eb8f
;rcx=actual direction XYZ,rdx=actual61C3A4. Preserve inputs on x87 across stores.
sgn_source_light_rotate_44eb8f PROC
    fld DWORD PTR [rcx]
    fld DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fld DWORD PTR [rdx+32]
    fmul st(0),st(1)
    fld DWORD PTR [rdx+16]
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld DWORD PTR [rdx+0]
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+0]
    fld DWORD PTR [rdx+36]
    fmul st(0),st(1)
    fld DWORD PTR [rdx+20]
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld DWORD PTR [rdx+4]
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx+40]
    fmul st(0),st(1)
    fld DWORD PTR [rdx+24]
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld DWORD PTR [rdx+8]
    fmul st(0),st(4)
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+8]
    fstp st(0)
    fstp st(0)
    fstp st(0)
    ret
sgn_source_light_rotate_44eb8f ENDP

PUBLIC sgn_source_model_light_basis_44efd0
;rcx=fresh stack destination (no alias),rdx=inverse outer,r8=inverse part.
sgn_source_model_light_basis_44efd0 PROC
    fld DWORD PTR [r8+32]
    fmul DWORD PTR [rdx+8]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+16]
    faddp st(1),st(0)
    fld DWORD PTR [r8]
    fmul DWORD PTR [rdx+0]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+0]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+20]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rdx+0]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [r8+36]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+24]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [rdx+0]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [r8+40]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+8]
    fld DWORD PTR [r8+32]
    fmul DWORD PTR [rdx+24]
    fld DWORD PTR [rdx+20]
    fmul DWORD PTR [r8+16]
    faddp st(1),st(0)
    fld DWORD PTR [r8]
    fmul DWORD PTR [rdx+16]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+16]
    fld DWORD PTR [rdx+20]
    fmul DWORD PTR [r8+20]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rdx+16]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+24]
    fmul DWORD PTR [r8+36]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+20]
    fld DWORD PTR [rdx+20]
    fmul DWORD PTR [r8+24]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [rdx+16]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+24]
    fmul DWORD PTR [r8+40]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+24]
    fld DWORD PTR [r8+32]
    fmul DWORD PTR [rdx+40]
    fld DWORD PTR [rdx+36]
    fmul DWORD PTR [r8+16]
    faddp st(1),st(0)
    fld DWORD PTR [r8]
    fmul DWORD PTR [rdx+32]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+32]
    fld DWORD PTR [rdx+36]
    fmul DWORD PTR [r8+20]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rdx+32]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+40]
    fmul DWORD PTR [r8+36]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+36]
    fld DWORD PTR [rdx+36]
    fmul DWORD PTR [r8+24]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [rdx+32]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+40]
    fmul DWORD PTR [r8+40]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+40]
    ret
sgn_source_model_light_basis_44efd0 ENDP
PUBLIC sgn_source_model_light_camera_42b406
sgn_source_model_light_camera_42b406 PROC
    fld DWORD PTR [rcx+0]
    fmul DWORD PTR [light_scale]
    fstp DWORD PTR [rdx+0]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [light_scale]
    fstp DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [light_scale]
    fstp DWORD PTR [rdx+8]
    ret
sgn_source_model_light_camera_42b406 ENDP
PUBLIC sgn_source_model_light_position_42b462
;part XYZ,outer XYZ,out XYZ,actual624574 offset. Last Z spill follows reset.
sgn_source_model_light_position_42b462 PROC
    fld DWORD PTR [rcx]
    fadd DWORD PTR [rdx]
    fstp DWORD PTR [r8]
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [rdx+4]
    fstp DWORD PTR [r8+4]
    fld DWORD PTR [rcx+8]
    fadd DWORD PTR [rdx+8]
    mov DWORD PTR [r9],0
    fstp DWORD PTR [r8+8]
    ret
sgn_source_model_light_position_42b462 ENDP
PUBLIC sgn_source_model_light_record_42b499
;rcx=actual light,rdx=actual prepared36,r8=rotation,r9=scaled-camera3+position3.
;5th argument is actual624574 offset; publish +24h at42B4BD, before first output.
sgn_source_model_light_record_42b499 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r11,QWORD PTR [rsp+64]
    FLD DWORD PTR [rcx + 030h] ;0042b499
    FMUL DWORD PTR [light_scale] ;0042b49c
    FLD DWORD PTR [rcx + 034h] ;0042b4a2
    FMUL DWORD PTR [light_scale] ;0042b4a5
    FLD DWORD PTR [rcx + 038h] ;0042b4ab
    FMUL DWORD PTR [light_scale] ;0042b4ae
    FLD st(2) ;0042b4b4
    FMUL DWORD PTR [r8] ;0042b4b6
    FLD st(1) ;0042b4b8
    FMUL DWORD PTR [r8 + 020h] ;0042b4ba
    add DWORD PTR [r11],24h ;0042b4bd
    faddp st(1),st(0) ;0042b4c2
    FLD st(2) ;0042b4c4
    FMUL DWORD PTR [r8 + 010h] ;0042b4c6
    faddp st(1),st(0) ;0042b4c9
    FSTP DWORD PTR [rdx+0] ;0042b4cb
    FLD st(2) ;0042b4ce
    FMUL DWORD PTR [r8 + 04h] ;0042b4d0
    FLD st(1) ;0042b4d3
    FMUL DWORD PTR [r8 + 024h] ;0042b4d5
    faddp st(1),st(0) ;0042b4d8
    FLD st(2) ;0042b4da
    FMUL DWORD PTR [r8 + 014h] ;0042b4dc
    faddp st(1),st(0) ;0042b4df
    FSTP DWORD PTR [rdx+4] ;0042b4e1
    FXCH st(2) ;0042b4e4
    FMUL DWORD PTR [r8 + 08h] ;0042b4e6
    FXCH st(2) ;0042b4e9
    FMUL DWORD PTR [r8 + 028h] ;0042b4eb
    FADDP st(2),st(0) ;0042b4ee
    FMUL DWORD PTR [r8 + 018h] ;0042b4f0
    faddp st(1),st(0) ;0042b4f3
    FSTP DWORD PTR [rdx+8] ;0042b4f5
    FLD DWORD PTR [rcx + 010h] ;0042b4f8
    FSUB DWORD PTR [r9+12] ;0042b4fb
    FSTP DWORD PTR [rcx + 020h] ;0042b4fe
    FLD DWORD PTR [rcx + 014h] ;0042b501
    FSUB DWORD PTR [r9+16] ;0042b504
    FSTP DWORD PTR [rcx + 024h] ;0042b507
    FLD DWORD PTR [rcx + 018h] ;0042b50a
    FSUB DWORD PTR [r9+20] ;0042b50d
    FSTP DWORD PTR [rcx + 028h] ;0042b510
    FLD DWORD PTR [r9] ;0042b513
    FADD DWORD PTR [rdx+0] ;0042b516
    FLD DWORD PTR [r9+4] ;0042b519
    FADD DWORD PTR [rdx+4] ;0042b51c
    FSTP DWORD PTR [rsp+16] ;0042b51f
    FLD DWORD PTR [r9+8] ;0042b522
    FADD DWORD PTR [rdx+8] ;0042b525
    FSTP DWORD PTR [rsp+20] ;0042b528
    FMUL DWORD PTR [light_half_scale] ;0042b52b
    FLD DWORD PTR [rsp+16] ;0042b531
    FMUL DWORD PTR [light_half_scale] ;0042b534
    FSTP DWORD PTR [rsp+16] ;0042b53a
    FLD DWORD PTR [rsp+20] ;0042b53d
    FMUL DWORD PTR [light_half_scale] ;0042b540
    FSTP DWORD PTR [rsp+20] ;0042b546
    FMUL DWORD PTR [light_half] ;0042b549
    FSTP DWORD PTR [rdx+16] ;0042b54f
    FLD DWORD PTR [rsp+16] ;0042b552
    FMUL DWORD PTR [light_half] ;0042b555
    FSTP DWORD PTR [rdx+20] ;0042b55b
    FLD DWORD PTR [rsp+20] ;0042b55e
    FMUL DWORD PTR [light_half] ;0042b561
    FSTP DWORD PTR [rdx+24] ;0042b567
    add rsp,24
    ret
sgn_source_model_light_record_42b499 ENDP
PUBLIC sgn_source_model_light_dot
;actual preparedXYZ,normalXYZ,r8d=specular-incidence operand order.
;Return raw binary32 result, including sign of zero.
sgn_source_model_light_dot PROC FRAME
    sub rsp,8
    .allocstack 8
    .endprolog
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rdx+8]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rdx+4]
    faddp st(1),st(0)
    test r8d,r8d
    jnz specular_dot_x
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    jmp dot_finish
specular_dot_x:
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx]
dot_finish:
    faddp st(1),st(0)
    fstp DWORD PTR [rsp]
    mov eax,DWORD PTR [rsp]
    add rsp,8
    ret
sgn_source_model_light_dot ENDP
PUBLIC sgn_source_model_light_ftol
sgn_source_model_light_ftol PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp+16],ecx
    fld DWORD PTR [rsp+16]
    source_model_ftol
    add rsp,24
    ret
sgn_source_model_light_ftol ENDP
PUBLIC sgn_source_model_light_point
;actual light144 and vertexXYZ -> low DWORD signed64 FTOL intensity.
sgn_source_model_light_point PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rcx+20h]
    fsub DWORD PTR [rdx]
    fld DWORD PTR [rcx+24h]
    fsub DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+28h]
    fsub DWORD PTR [rdx+8]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [light_intensity]
    fld st(1)
    fmul st(0),st(2)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fld st(4)
    fmul st(0),st(5)
    faddp st(1),st(0)
    fdivp st(1),st(0)
    source_model_ftol
    fstp st(0)
    fstp st(0)
    fstp st(0)
    add rsp,24
    ret
sgn_source_model_light_point ENDP

PUBLIC sgn_source_light_fade_42d88f
;rcx sole477B80/477B84/589E00 fields;edx forced active-overlay/split path.
;Source42D870 numeric/stores retain unspilled FST values for clamps.
;Return raw binary32 gamma argument; C++ calls existing4320C0 after return.
sgn_source_light_fade_42d88f PROC FRAME
    sub rsp,8
    .allocstack 8
    .endprolog
    test edx,edx
    jnz source_fade_forced
    FLD DWORD PTR [rcx] ;0042d88f
    FSUB DWORD PTR [rcx+4] ;0042d895
    FLD st(0) ;0042d89b
    FABS ;0042d89d
    FCOMP DWORD PTR [fade_delta] ;0042d89f
    FNSTSW AX ;0042d8a5
    TEST AH,041h ;0042d8a7
    JNZ fade_0042d8d5 ;0042d8aa
    FCOMP DWORD PTR [fade_zero] ;0042d8ac
    FNSTSW AX ;0042d8b2
    TEST AH,041h ;0042d8b4
    JNZ fade_0042d8c1 ;0042d8b7
    FLD DWORD PTR [light_half] ;0042d8b9
    JMP fade_0042d8c7 ;0042d8bf
fade_0042d8c1:
    FLD DWORD PTR [fade_current_down] ;0042d8c1
fade_0042d8c7:
    FLD DWORD PTR [rcx+4] ;0042d8c7
    FADD st(0),st(1) ;0042d8cd
    FSTP DWORD PTR [rcx+4] ;0042d8cf
fade_0042d8d5:
    FSTP st(0) ;0042d8d5
    FLD DWORD PTR [rcx+4] ;0042d8d7
    FCOMP DWORD PTR [fade_current_threshold] ;0042d8dd
    FLD DWORD PTR [rcx+8] ;0042d8e3
    FNSTSW AX ;0042d8e9
    TEST AH,041h ;0042d8eb
    JNZ fade_0042d915 ;0042d8ee
    FADD DWORD PTR [fade_blend_up] ;0042d8f0
    FST DWORD PTR [rcx+8] ;0042d8f6
    FCOMP DWORD PTR [light_one] ;0042d8fc
    FNSTSW AX ;0042d902
    TEST AH,041h ;0042d904
    JNZ fade_0042d938 ;0042d907
    MOV DWORD PTR [rcx+8],03f800000h ;0042d909
    JMP fade_0042d938 ;0042d913
fade_0042d915:
    FSUB DWORD PTR [fade_blend_down] ;0042d915
    FST DWORD PTR [rcx+8] ;0042d91b
    FCOMP DWORD PTR [fade_zero] ;0042d921
    FNSTSW AX ;0042d927
    TEST AH,01h ;0042d929
    JZ fade_0042d938 ;0042d92c
    MOV DWORD PTR [rcx+8],00h ;0042d92e
fade_0042d938:
    FLD DWORD PTR [rcx+4] ;0042d938
    FSUB DWORD PTR [rcx+8] ;0042d93e
    fstp DWORD PTR [rsp]
    mov eax,DWORD PTR [rsp]
    add rsp,8
    ret
source_fade_forced:
    mov DWORD PTR [rcx+4],03f800000h
    mov DWORD PTR [rcx],03f800000h
    mov eax,03f800000h
    add rsp,8
    ret
sgn_source_light_fade_42d88f ENDP
END
