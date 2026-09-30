option casemap:none
.const
k46e1ec DWORD 000000000h ;PE0046E1EC
k46e264 DWORD 03f800000h ;PE0046E264
k46e368 DWORD 042855555h ;PE0046E368
k46e548 DWORD 03cc49ba6h ;PE0046E548
k46e444 DWORD 03b808081h ;PE0046E444
k46e54c DWORD 0332bcc77h ;PE0046E54C
k46e53c DWORD 0428c0000h ;PE0046E53C
k46e554 DWORD 03983126fh ;PE0046E554
k46e230 DWORD 03dcccccdh ;PE0046E230
k46e294 DWORD 03c23d70ah ;PE0046E294
k46e550 DWORD 043400000h ;PE0046E550
k46eba0 DWORD 041b60b61h ;PE0046EBA0
.code

PUBLIC sgn_vdp_normalize_44ed40
sgn_vdp_normalize_44ed40 PROC
fld dword ptr [rcx+8] ;0044ED40
    fld dword ptr [rcx+4] ;0044ED43
    fld dword ptr [rcx] ;0044ED46
    fld st(0) ;0044ED48
    fmul st(0),st(1) ;0044ED4A
    fld st(2) ;0044ED4C
    fmul st(0),st(3) ;0044ED4E
    faddp st(1),st(0) ;0044ED50
    fld st(3) ;0044ED52
    fmul st(0),st(4) ;0044ED54
    faddp st(1),st(0) ;0044ED56
    fstp st(3) ;0044ED58
    fstp st(0) ;0044ED5A
    fstp st(0) ;0044ED5C
    fcom dword ptr [k46e1ec] ;0044ED5E
    fnstsw ax
    test ah,40h
    jz norm_nonzero
    xor eax,eax
    fstp st(0)
    mov DWORD PTR [rcx+8],eax
    mov DWORD PTR [rcx+4],eax
    mov DWORD PTR [rcx],eax
    ret
norm_nonzero:
    fsqrt ;0044ED78
    fdivr dword ptr [k46e264] ;0044ED7A
    fld st(0) ;0044ED80
    fmul dword ptr [rcx] ;0044ED82
    fstp dword ptr [rcx] ;0044ED84
    fld st(0) ;0044ED86
    fmul dword ptr [rcx+4] ;0044ED88
    fstp dword ptr [rcx+4] ;0044ED8B
    fmul dword ptr [rcx+8] ;0044ED8E
    fstp dword ptr [rcx+8] ;0044ED91
    ret
sgn_vdp_normalize_44ed40 ENDP

PUBLIC sgn_vdp_steering_44f800
sgn_vdp_steering_44f800 PROC FRAME
sub rsp,24
    .allocstack 24
    .endprolog
    mov DWORD PTR [rsp+16],edx
    fld DWORD PTR [rsp+16]
    fmul DWORD PTR [k46eba0]
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ah,0ch
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
    and eax,1fffh
    fld DWORD PTR [r8+rax*4+8192]
    fstp DWORD PTR [rsp+4]
    fld DWORD PTR [r8+rax*4]
    fstp DWORD PTR [rsp+16]
    mov edx,DWORD PTR [rsp+4]
    mov r9d,DWORD PTR [rsp+16]
    fld DWORD PTR [rsp+16]
    fchs
    fstp DWORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp+8]
    mov DWORD PTR [rcx],edx
    mov DWORD PTR [rcx+4],0
    mov DWORD PTR [rcx+8],eax
    mov DWORD PTR [rcx+12],0
    mov DWORD PTR [rcx+16],0
    mov DWORD PTR [rcx+20],3f800000h
    mov DWORD PTR [rcx+24],0
    mov DWORD PTR [rcx+28],0
    mov DWORD PTR [rcx+32],r9d
    mov DWORD PTR [rcx+36],0
    mov DWORD PTR [rcx+40],edx
    mov DWORD PTR [rcx+44],0
    mov DWORD PTR [rcx+48],0
    mov DWORD PTR [rcx+52],0
    mov DWORD PTR [rcx+56],0
    mov DWORD PTR [rcx+60],3f800000h
    add rsp,24
    ret
sgn_vdp_steering_44f800 ENDP

PUBLIC sgn_vdp_multiply_44efd0
sgn_vdp_multiply_44efd0 PROC
;RCX unique local destination, RDX actual wheel rotation, R8 local steering.
    ;The only qualified callsite has three disjoint buffers; W stays untouched.
    mov r9d,3
matrix_column:
    fld DWORD PTR [r8+32]
    fmul DWORD PTR [rdx+8]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+16]
    faddp st(1),st(0)
    fld DWORD PTR [r8]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+20]
    fld DWORD PTR [r8+4]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [r8+36]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [r8+24]
    fld DWORD PTR [r8+8]
    fmul DWORD PTR [rdx]
    faddp st(1),st(0)
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [r8+40]
    faddp st(1),st(0)
    fstp DWORD PTR [rcx+8]
    add rcx,16
    add rdx,16
    dec r9d
    jnz matrix_column
    ret
sgn_vdp_multiply_44efd0 ENDP

PUBLIC sgn_vdp_cross_413b60
sgn_vdp_cross_413b60 PROC
fld dword ptr [rdx+4] ;00413B7B
    fmul dword ptr [rdx+24] ;00413B7E
    fld dword ptr [rdx+8] ;00413B81
    fmul dword ptr [rdx+20] ;00413B84
    fsubp st(1),st(0) ;00413B8A
    fstp dword ptr [rcx+0] ;00413B8C
    fld dword ptr [rdx+8] ;00413B8F
    fmul dword ptr [rdx+16] ;00413B92
    fld dword ptr [rdx+24] ;00413B95
    fmul dword ptr [rdx] ;00413B98
    fsubp st(1),st(0) ;00413B9A
    fstp dword ptr [rcx+4] ;00413B9C
    fld dword ptr [rdx+20] ;00413B9F
    fmul dword ptr [rdx] ;00413BA2
    fld dword ptr [rdx+4] ;00413BA4
    fmul dword ptr [rdx+16] ;00413BA7
    fsubp st(1),st(0) ;00413BAA
    fstp dword ptr [rcx+8] ;00413BAC
    fld dword ptr [rcx+4] ;00413BAF
    fmul dword ptr [rdx+8] ;00413BB2
    fld dword ptr [rcx+8] ;00413BB5
    fmul dword ptr [rdx+4] ;00413BB8
    fsubp st(1),st(0) ;00413BBB
    fstp dword ptr [rcx+16] ;00413BBD
    fld dword ptr [rcx+8] ;00413BC0
    fmul dword ptr [rdx] ;00413BC3
    fld dword ptr [rcx+0] ;00413BC5
    fmul dword ptr [rdx+8] ;00413BC8
    fsubp st(1),st(0) ;00413BCB
    fstp dword ptr [rcx+20] ;00413BCD
    fld dword ptr [rcx+0] ;00413BD0
    fmul dword ptr [rdx+4] ;00413BD3
    fld dword ptr [rcx+4] ;00413BD6
    fmul dword ptr [rdx] ;00413BD9
    fsubp st(1),st(0) ;00413BDB
    fstp dword ptr [rcx+24] ;00413BDD
    ret
sgn_vdp_cross_413b60 ENDP

PUBLIC sgn_vdp_segment_start
sgn_vdp_segment_start PROC
mov r9,QWORD PTR [rdx]
    mov r8,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    fld dword ptr [r8+32] ;00413E80
    fmul dword ptr [r9+8] ;00413EA6
    fld dword ptr [r8+16] ;00413EA9
    fmul dword ptr [r9+4] ;00413EAF
    faddp st(1),st(0) ;00413EB8
    fld dword ptr [r8+0] ;00413EBA
    fmul dword ptr [r9] ;00413EC0
    faddp st(1),st(0) ;00413EC3
    fld dword ptr [r8+36] ;00413EC5
    fmul dword ptr [r9+8] ;00413ECB
    fld dword ptr [r8+20] ;00413ECE
    fmul dword ptr [r9+4] ;00413ED4
    faddp st(1),st(0) ;00413ED7
    fld dword ptr [r8+4] ;00413ED9
    fmul dword ptr [r9] ;00413EDF
    faddp st(1),st(0) ;00413EE2
    fstp dword ptr [rcx+4] ;00413EE4
    fld dword ptr [r8+40] ;00413EE7
    fmul dword ptr [r9+8] ;00413EED
    fld dword ptr [r8+24] ;00413EF0
    fmul dword ptr [r9+4] ;00413EF6
    faddp st(1),st(0) ;00413EFC
    fld dword ptr [r8+8] ;00413EFE
    fmul dword ptr [r9] ;00413F04
    faddp st(1),st(0) ;00413F07
    fstp dword ptr [rcx+8] ;00413F09
    fstp dword ptr [rcx] ;00413F0F
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [r10]
    fstp DWORD PTR [rcx+4]
    ret
sgn_vdp_segment_start ENDP

PUBLIC sgn_vdp_segment_step
sgn_vdp_segment_step PROC
mov r9,QWORD PTR [rdx]
    mov r8,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    fld dword ptr [r8+32] ;00413F39
    fmul dword ptr [r9+8] ;00413F66
    fld dword ptr [r8+16] ;00413F69
    fmul dword ptr [r9+4] ;00413F6F
    faddp st(1),st(0) ;00413F7B
    fld dword ptr [r8+0] ;00413F7D
    fmul dword ptr [r9] ;00413F83
    faddp st(1),st(0) ;00413F86
    fstp dword ptr [rcx+16] ;00413F88
    fld dword ptr [r8+36] ;00413F8B
    fmul dword ptr [r9+8] ;00413F91
    fld dword ptr [r8+20] ;00413F94
    fmul dword ptr [r9+4] ;00413F9A
    faddp st(1),st(0) ;00413FA2
    fld dword ptr [r8+4] ;00413FA4
    fmul dword ptr [r9] ;00413FAA
    faddp st(1),st(0) ;00413FAD
    fstp dword ptr [rcx+20] ;00413FAF
    fld dword ptr [r8+40] ;00413FB2
    fmul dword ptr [r9+8] ;00413FB8
    fld dword ptr [r8+24] ;00413FBB
    fmul dword ptr [r9+4] ;00413FC1
    faddp st(1),st(0) ;00413FCA
    fld dword ptr [r8+8] ;00413FCC
    fmul dword ptr [r9] ;00413FD2
    faddp st(1),st(0) ;00413FD5
    fstp dword ptr [rcx+24] ;00413FD7
    fld DWORD PTR [r10]
    fmul DWORD PTR [rcx+16]
    fld DWORD PTR [r10]
    fmul DWORD PTR [rcx+20]
    fstp DWORD PTR [rcx+36]
    fld DWORD PTR [r10]
    fmul DWORD PTR [rcx+24]
    mov eax,DWORD PTR [r9+12]
    mov DWORD PTR [rcx+28],eax
    fstp DWORD PTR [rcx+40]
    fld DWORD PTR [rcx]
    fadd st(0),st(1)
    fstp DWORD PTR [rcx]
    fstp st(0)
    fld DWORD PTR [rcx+4]
    fadd DWORD PTR [rcx+36]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fadd DWORD PTR [rcx+40]
    fstp DWORD PTR [rcx+8]
    ret
sgn_vdp_segment_step ENDP

PUBLIC sgn_vdp_segment_world
sgn_vdp_segment_world PROC FRAME
sub rsp,88
    .allocstack 88
    .endprolog
    mov r8,QWORD PTR [rdx]
    mov r9,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    mov r11,QWORD PTR [rdx+24]
    mov rax,QWORD PTR [rdx+32]
    fld dword ptr [r8] ;0041404C
    fchs ;00414055
    fmul dword ptr [r9] ;00414057
    fld dword ptr [r8] ;00414063
    fchs ;00414066
    fld st(0) ;00414068
    fmul dword ptr [r9+4] ;0041406A
    fstp dword ptr [rsp+48] ;0041407C
    fmul dword ptr [r9+8] ;0041407F
    fstp dword ptr [rsp+52] ;00414085
    fld dword ptr [k46e53c] ;00414088
    fsub dword ptr [r8+4] ;0041408E
    fld st(0) ;00414091
    fmul dword ptr [r10] ;00414093
    fstp dword ptr [rsp+60] ;00414099
    fld st(0) ;0041409C
    fmul dword ptr [r10+4] ;0041409E
    fstp dword ptr [rsp+64] ;004140A4
    fmul dword ptr [r10+8] ;004140A7
    fstp dword ptr [rsp+68] ;004140AD
    fld dword ptr [r8+8] ;004140B0
    fmul dword ptr [r11] ;004140B3
    fstp dword ptr [rsp+12] ;004140B9
    fld dword ptr [r8+8] ;004140BC
    fmul dword ptr [r11+4] ;004140BF
    fstp dword ptr [rsp+16] ;004140C5
    fld dword ptr [r8+8] ;004140C8
    fmul dword ptr [r11+8] ;004140CB
    fstp dword ptr [rsp+20] ;004140D1
    fadd dword ptr [rsp+60] ;004140D4
    fld dword ptr [rsp+48] ;004140D7
    fadd dword ptr [rsp+64] ;004140DA
    fstp dword ptr [rsp+48] ;004140DD
    fld dword ptr [rsp+52] ;004140E0
    fadd dword ptr [rsp+68] ;004140E3
    fstp dword ptr [rsp+52] ;004140E6
    fadd dword ptr [rsp+12] ;004140E9
    fld dword ptr [rsp+16] ;004140EC
    fadd dword ptr [rsp+48] ;004140EF
    fstp dword ptr [rsp+48] ;004140F2
    fld dword ptr [rsp+20] ;004140F5
    fadd dword ptr [rsp+52] ;004140F8
    fstp dword ptr [rsp+52] ;004140FB
    fld dword ptr [rax+0] ;004140FE
    fsub st(0),st(1) ;00414104
    fstp dword ptr [rcx+0] ;00414106
    fstp st(0) ;0041410C
    fld dword ptr [rax+4] ;0041410E
    fsub dword ptr [rsp+48] ;00414114
    fstp dword ptr [rcx+4] ;00414117
    fld dword ptr [rax+8] ;0041411D
    fsub dword ptr [rsp+52] ;00414123
    fstp dword ptr [rcx+8] ;00414126
    add rsp,88
    ret
sgn_vdp_segment_world ENDP

PUBLIC sgn_vdp_anchor
sgn_vdp_anchor PROC
mov r9,QWORD PTR [rdx]
    mov r8,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    fld dword ptr [r9+4] ;004141BE
    fadd dword ptr [r10] ;004141C1
    fld dword ptr [r8+32] ;004141CA
    fmul dword ptr [r9+8] ;004141D6
    fld dword ptr [r8+0] ;004141D9
    fmul dword ptr [r9] ;004141DF
    faddp st(1),st(0) ;004141E5
    fld dword ptr [r8+16] ;004141E7
    fmul st(0),st(2) ;004141ED
    faddp st(1),st(0) ;004141EF
    fstp dword ptr [rcx] ;004141F1
    fld dword ptr [r8+36] ;004141F4
    fmul dword ptr [r9+8] ;004141FA
    fld dword ptr [r8+4] ;004141FD
    fmul dword ptr [r9] ;00414203
    faddp st(1),st(0) ;0041420B
    fld dword ptr [r8+20] ;0041420D
    fmul st(0),st(2) ;00414213
    faddp st(1),st(0) ;00414215
    fstp dword ptr [rcx+4] ;00414217
    fld dword ptr [r8+40] ;0041421A
    fmul dword ptr [r9+8] ;00414220
    fld dword ptr [r8+8] ;00414223
    fmul dword ptr [r9] ;00414229
    faddp st(1),st(0) ;00414232
    fld dword ptr [r8+24] ;00414234
    fmul st(0),st(2) ;0041423A
    faddp st(1),st(0) ;0041423C
    fstp dword ptr [rcx+8] ;0041423E
    fstp st(0)
    ret
sgn_vdp_anchor ENDP

PUBLIC sgn_vdp_project_415190
sgn_vdp_project_415190 PROC FRAME
sub rsp,88
    .allocstack 88
    .endprolog
    mov r9,QWORD PTR [rdx]
    mov r8,QWORD PTR [rdx+8]
    mov r10,QWORD PTR [rdx+16]
    mov r11,QWORD PTR [rdx+24]
    mov rax,QWORD PTR [rdx+32]
    mov QWORD PTR [rsp+8],rdx
    fld dword ptr [r9+4] ;00415196
    fmul dword ptr [r8+16] ;00415199
    fld dword ptr [r9+8] ;0041519F
    fmul dword ptr [r8+32] ;004151A2
    faddp st(1),st(0) ;004151B0
    fld dword ptr [r8+0] ;004151B2
    fmul dword ptr [r9] ;004151B8
    faddp st(1),st(0) ;004151BA
    fld dword ptr [r9+4] ;004151BC
    fmul dword ptr [r8+20] ;004151BF
    fld dword ptr [r9+8] ;004151C5
    fmul dword ptr [r8+36] ;004151C8
    faddp st(1),st(0) ;004151CE
    fld dword ptr [r8+4] ;004151D0
    fmul dword ptr [r9] ;004151D6
    faddp st(1),st(0) ;004151D8
    fstp dword ptr [rsp+52] ;004151DA
    fld dword ptr [r9+4] ;004151DD
    fmul dword ptr [r8+24] ;004151E0
    fld dword ptr [r9+8] ;004151E6
    fmul dword ptr [r8+40] ;004151E9
    faddp st(1),st(0) ;004151EF
    fld dword ptr [r8+8] ;004151F1
    fmul dword ptr [r9] ;004151F7
    faddp st(1),st(0) ;004151F9
    fstp dword ptr [rsp+56] ;004151FB
    mov rdx,QWORD PTR [rdx+48]
    fchs ;004151FE
    fld st(0) ;00415200
    fmul dword ptr [r10+0] ;00415202
    fstp dword ptr [rsp+64] ;00415208
    fld st(0) ;0041520B
    fmul dword ptr [r10+4] ;0041520D
    fstp dword ptr [rsp+68] ;00415213
    fmul dword ptr [r10+8] ;00415216
    fld dword ptr [k46e53c] ;0041521C
    fsub dword ptr [rsp+52] ;00415222
    fsub dword ptr [rdx] ;00415225
    fld st(0) ;0041522B
    fmul dword ptr [r11+0] ;0041522D
    fstp dword ptr [rsp+16] ;00415233
    fld st(0) ;00415236
    fmul dword ptr [r11+4] ;00415238
    fstp dword ptr [rsp+20] ;0041523E
    fmul dword ptr [r11+8] ;00415241
    fld dword ptr [rsp+56] ;00415247
    fmul dword ptr [rax+0] ;0041524A
    fstp dword ptr [rsp+32] ;00415250
    fld dword ptr [rsp+56] ;00415253
    fmul dword ptr [rax+4] ;00415256
    fstp dword ptr [rsp+36] ;0041525C
    fld dword ptr [rsp+56] ;0041525F
    fmul dword ptr [rax+8] ;00415262
    fstp dword ptr [rsp+40] ;00415268
    fld dword ptr [rsp+16] ;0041526B
    fadd dword ptr [rsp+64] ;0041526E
    fstp dword ptr [rsp+64] ;00415271
    fld dword ptr [rsp+20] ;00415274
    fadd dword ptr [rsp+68] ;00415277
    fstp dword ptr [rsp+68] ;0041527A
    faddp st(1),st(0) ;0041527D
    fld dword ptr [rsp+32] ;0041527F
    fadd dword ptr [rsp+64] ;00415282
    fstp dword ptr [rsp+64] ;00415285
    fld dword ptr [rsp+36] ;00415288
    fadd dword ptr [rsp+68] ;0041528B
    fstp dword ptr [rsp+68] ;0041528E
    fadd dword ptr [rsp+40] ;00415291
    mov r8,QWORD PTR [rsp+8]
    mov r8,QWORD PTR [r8+40]
    fld dword ptr [r8+0] ;00415294
    fsub dword ptr [rsp+64] ;0041529A
    fstp dword ptr [rcx+0] ;0041529D
    fld dword ptr [r8+4] ;0041529F
    fsub dword ptr [rsp+68] ;004152A5
    fstp dword ptr [rcx+4] ;004152A8
    fld dword ptr [r8+8] ;004152AB
    fsub st(0),st(1) ;004152B1
    fstp dword ptr [rcx+8] ;004152B3
    fstp st(0) ;004152B6
    add rsp,88
    ret
sgn_vdp_project_415190 ENDP

PUBLIC sgn_vdp_speed_alpha
sgn_vdp_speed_alpha PROC
fld DWORD PTR [rcx]
    fsub DWORD PTR [k46e368]
    fcom DWORD PTR [k46e1ec]
    fnstsw ax
    test ah,1
    jnz speed_none
    fmul DWORD PTR [k46e548]
    fst DWORD PTR [rdx]
    fcomp DWORD PTR [k46e264]
    fnstsw ax
    test ah,41h
    jnz speed_emit
    mov DWORD PTR [rdx],3f800000h
speed_emit:
    mov eax,1
    ret
speed_none:
    fstp st(0)
    xor eax,eax
    ret
sgn_vdp_speed_alpha ENDP

PUBLIC sgn_vdp_launch_alpha
sgn_vdp_launch_alpha PROC FRAME
sub rsp,8
    .allocstack 8
    .endprolog
    fild DWORD PTR [rcx]
    fmul DWORD PTR [k46e444]
    fstp DWORD PTR [rsp]
    mov eax,DWORD PTR [rsp]
    add rsp,8
    ret
sgn_vdp_launch_alpha ENDP

PUBLIC sgn_vdp_distance_alpha
sgn_vdp_distance_alpha PROC FRAME
sub rsp,8
    .allocstack 8
    .endprolog
    fld dword ptr [rcx+0] ;004153DF
    fsub dword ptr [rdx+0] ;004153E5
    fld dword ptr [rcx+4] ;004153E8
    fsub dword ptr [rdx+4] ;004153EE
    fld dword ptr [rcx+8] ;004153F1
    fsub dword ptr [rdx+8] ;004153F7
    fld st(0) ;00415400
    fmul st(0),st(1) ;00415402
    fld st(2) ;00415404
    fmul st(0),st(3) ;00415406
    faddp st(1),st(0) ;00415408
    fld st(3) ;0041540A
    fmul st(0),st(4) ;0041540C
    faddp st(1),st(0) ;0041540E
    fmul dword ptr [k46e54c] ;00415410
    fstp dword ptr [rsp] ;00415416
    fstp st(0) ;00415419
    fstp st(0) ;0041541B
    fstp st(0) ;0041541D
    fld dword ptr [rsp] ;0041541F
    fcomp dword ptr [k46e1ec] ;00415422
    fnstsw ax
    test ah,1
    jz distance_upper
    mov DWORD PTR [rsp],0
    jmp distance_done
distance_upper:
    fld DWORD PTR [rsp]
    fcomp DWORD PTR [k46e264]
    fnstsw ax
    test ah,41h
    jnz distance_done
    mov DWORD PTR [rsp],3f800000h
distance_done:
    mov eax,DWORD PTR [rsp]
    add rsp,8
    ret
sgn_vdp_distance_alpha ENDP

PUBLIC sgn_vdp_glow
sgn_vdp_glow PROC FRAME
sub rsp,24
    .allocstack 24
    .endprolog
    mov r8,QWORD PTR [rdx]
    mov r9,QWORD PTR [rdx+8]
    fld dword ptr [r8+0] ;00415535
    fsub dword ptr [r9+0] ;0041553B
    fld dword ptr [r8+4] ;0041553E
    fsub dword ptr [r9+4] ;00415544
    fld dword ptr [r8+8] ;00415547
    fsub dword ptr [r9+8] ;0041554D
    fld st(0) ;00415550
    fmul st(0),st(1) ;00415552
    fld st(2) ;00415554
    fmul st(0),st(3) ;00415556
    faddp st(1),st(0) ;00415558
    fld st(3) ;0041555A
    fmul st(0),st(4) ;0041555C
    faddp st(1),st(0) ;0041555E
    fsqrt ;00415560
    fstp st(3) ;00415562
    fstp st(0) ;00415564
    fstp st(0) ;00415566
    fmul dword ptr [k46e554] ;00415568
    fadd dword ptr [k46e230] ;0041556E
    fcom dword ptr [k46e264] ;00415574
    fnstsw ax
    test ah,41h
    jnz glow_unclamped
    fstp st(0)
    fld DWORD PTR [k46e264]
glow_unclamped:
    mov r8,QWORD PTR [rdx+16]
    mov r10d,DWORD PTR [r8]
    mov r9,QWORD PTR [rdx+24]
    mov r11d,DWORD PTR [r9]
    neg r11d
    mov r8,QWORD PTR [rdx+32]
    fld DWORD PTR [r8+r10*4]
    fmul DWORD PTR [k46e294]
    fld st(1)
    fadd st(0),st(0)
    and r11d,1fffh
    faddp st(1),st(0)
    fxch st(1)
    fmul DWORD PTR [k46e550]
    fstcw WORD PTR [rsp]
    fwait
    mov ax,WORD PTR [rsp]
    or ah,0ch
    mov WORD PTR [rsp+2],ax
    fldcw WORD PTR [rsp+2]
    fistp QWORD PTR [rsp+8]
    fldcw WORD PTR [rsp]
    mov eax,DWORD PTR [rsp+8]
    mov WORD PTR [rcx+6],ax
    fld DWORD PTR [r8+r11*4+8192]
    mov DWORD PTR [rcx+80],0
    fstp DWORD PTR [rcx+72]
    fld DWORD PTR [r8+r11*4]
    fstp DWORD PTR [rcx+76]
    fst DWORD PTR [rcx+40]
    fst DWORD PTR [rcx+44]
    fstp DWORD PTR [rcx+48]
    ;462866/462872: source little-endian DWORD is oldCW:truncCW.
    ;Read the already captured words; no new x87 operation or control change.
    movzx eax,WORD PTR [rsp]
    shl eax,16
    mov ax,WORD PTR [rsp+2]
    add rsp,24
    ret
sgn_vdp_glow ENDP
END
