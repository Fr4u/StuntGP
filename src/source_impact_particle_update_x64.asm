option casemap:none
.const
impact_one DWORD 03f800000h ;PE46E264
impact_two DWORD 040000000h ;PE46E200
impact_three DWORD 040400000h ;415FA5/415FCB argument
impact_gravity_limit DWORD 046d9038eh ;PE46E204
impact_random_scale DWORD 038000100h ;PE46E714
impact_depth_scale DWORD 042000000h ;PE46E5FC
.code
PUBLIC sgn_source_impact_move_415f5a
;RCX actual position, RDX actual velocity;415F5A..415F73, XYZ only.
sgn_source_impact_move_415f5a PROC
    fld DWORD PTR [rdx]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rdx+4]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx+8]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
    ret
sgn_source_impact_move_415f5a ENDP

PUBLIC sgn_source_impact_gravity_401d70
;RCX actual velocity, RDX actual gravity. Reached scale argument is exactly1.
sgn_source_impact_gravity_401d70 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
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
    fcomp DWORD PTR [impact_gravity_limit]
    fstp st(0)
    fnstsw ax
    fstp st(0)
    test ah,41h
    fstp st(0)
    jz impact_gravity_done
    fld DWORD PTR [rdx]
    fmul DWORD PTR [impact_one]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [impact_one]
    fstp DWORD PTR [rsp]
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [impact_one]
    fstp DWORD PTR [rsp+4]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rsp]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rsp+4]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
impact_gravity_done:
    add rsp,24
    ret
sgn_source_impact_gravity_401d70 ENDP

PUBLIC sgn_source_impact_decay_415f82
;RCX actual velocity, RDX actual decay; reload decay for each source lane.
sgn_source_impact_decay_415f82 PROC
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
    ret
sgn_source_impact_decay_415f82 ENDP

impact_random_42f1c0 MACRO amplitude
    ;463289 publishes actual4959C0 before FILD of the signed result.
    mov eax,DWORD PTR [rdx]
    imul eax,eax,343fdh
    add eax,269ec3h
    mov DWORD PTR [rdx],eax
    sar eax,10h
    and eax,7fffh
    sub eax,3fffh
    mov DWORD PTR [rsp],eax
    fild DWORD PTR [rsp]
    fmul DWORD PTR [amplitude]
    fmul DWORD PTR [impact_random_scale]
ENDM
PUBLIC sgn_source_impact_jitter_415fa5
;RCX actual440-byte particle, RDX sole RNG DWORD. Keep42F1C0 return in ST0.
sgn_source_impact_jitter_415fa5 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    impact_random_42f1c0 impact_three
    fmul DWORD PTR [rcx+438h]
    fadd DWORD PTR [rcx+18h]
    fstp DWORD PTR [rcx+18h]
    impact_random_42f1c0 impact_two
    fabs
    fmul DWORD PTR [rcx+438h]
    fadd DWORD PTR [rcx+1ch]
    fstp DWORD PTR [rcx+1ch]
    impact_random_42f1c0 impact_three
    fmul DWORD PTR [rcx+438h]
    fadd DWORD PTR [rcx+20h]
    fstp DWORD PTR [rcx+20h]
    add rsp,24
    ret
sgn_source_impact_jitter_415fa5 ENDP

PUBLIC sgn_source_impact_length_415ebd
;RCX query_motion, RDX velocityW, R8 query.motion_payload, R9 travel_limit.
sgn_source_impact_length_415ebd PROC
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [rcx+4]
    mov eax,DWORD PTR [rdx]
    mov DWORD PTR [r8],eax
    faddp st(1),st(0)
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
    fsqrt
    fstp DWORD PTR [r9]
    ret
sgn_source_impact_length_415ebd ENDP

PUBLIC sgn_source_impact_reflect_421c30
;RCX actual query_motion, RDX actual last_normal. Reached restitution is1.
sgn_source_impact_reflect_421c30 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    fld DWORD PTR [rdx+8]
    fmul DWORD PTR [rcx+8]
    fld DWORD PTR [rdx+4]
    fmul DWORD PTR [rcx+4]
    faddp st(1),st(0)
    fld DWORD PTR [rdx]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
    fld DWORD PTR [impact_one]
    fsub DWORD PTR [impact_one]
    fsubr DWORD PTR [impact_two]
    fmulp st(1),st(0)
    fld DWORD PTR [rdx]
    fmul st(0),st(1)
    fstp DWORD PTR [rsp]
    fld DWORD PTR [rdx+4]
    fmul st(0),st(1)
    fstp DWORD PTR [rsp+4]
    fmul DWORD PTR [rdx+8]
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rsp]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rcx+4]
    fsub DWORD PTR [rsp+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rcx+8]
    fsub st(0),st(1)
    fstp DWORD PTR [rcx+8]
    fstp st(0)
    add rsp,24
    ret
sgn_source_impact_reflect_421c30 ENDP

PUBLIC sgn_source_impact_project_41e410
;RCX actual position, RDX actual cyclic output, R8 synchronous pointer bundle:
;world_view, projection, viewport, captured-current near. No float snapshots.
sgn_source_impact_project_41e410 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r10,QWORD PTR [r8]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [r10+10h]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [r10+20h]
    mov r9,QWORD PTR [r8+18h] ;captured source camera pointer,41E428
    faddp st(1),st(0)
    fld DWORD PTR [r10]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
    fadd DWORD PTR [r10+30h]
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [r10+14h]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [r10+24h]
    faddp st(1),st(0)
    fld DWORD PTR [r10+4]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
    fadd DWORD PTR [r10+34h]
    fstp DWORD PTR [rsp] ;41E463 Y spill
    fld DWORD PTR [rcx+4]
    fmul DWORD PTR [r10+18h]
    fld DWORD PTR [rcx+8]
    fmul DWORD PTR [r10+28h]
    faddp st(1),st(0)
    fld DWORD PTR [r10+8]
    fmul DWORD PTR [rcx]
    faddp st(1),st(0)
    fadd DWORD PTR [r10+38h]
    fst DWORD PTR [rsp+4] ;41E48A, unrounded depth retained for comparison
    fcomp DWORD PTR [r9]
    fnstsw ax
    test ah,1
    jz impact_project_visible
    fstp st(0)
    xor eax,eax
    add rsp,24
    ret
impact_project_visible:
    mov r10,QWORD PTR [r8+8]
    fmul DWORD PTR [r10]
    fld DWORD PTR [r10+14h]
    fmul DWORD PTR [rsp]
    mov eax,1
    fstp DWORD PTR [rsp+8] ;41E4B6 clipY spill
    fld DWORD PTR [rsp+4]
    fadd DWORD PTR [impact_one]
    fdivr DWORD PTR [impact_one]
    fstp DWORD PTR [rsp+12] ;41E4C8 inverse float spill
    mov r10,QWORD PTR [r8+10h]
    fld DWORD PTR [r10+18h]
    fmul DWORD PTR [rsp+12]
    fmulp st(1),st(0)
    fadd DWORD PTR [r10+10h]
    fld DWORD PTR [r10+1ch]
    fmul DWORD PTR [rsp+12]
    fmul DWORD PTR [rsp+8]
    fadd DWORD PTR [r10+14h]
    fstp DWORD PTR [rsp+8] ;41E4EE finalY spills before X store
    mov ecx,DWORD PTR [rsp+8]
    fstp DWORD PTR [rdx]
    fld DWORD PTR [rsp+12]
    fmul DWORD PTR [impact_depth_scale]
    mov DWORD PTR [rdx+4],ecx
    fstp DWORD PTR [rdx+8]
    add rsp,24
    ret
sgn_source_impact_project_41e410 ENDP
END
