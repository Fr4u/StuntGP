option casemap:none
.const
detail_scale DWORD 0404ccccdh ;PE46E6F8: source binary32 3.2
detail_mid DWORD 0459c4000h ;PE46E290:5000
detail_far DWORD 0461c4000h ;PE46E248:10000
detail_close DWORD 0453b8000h ;PE46E2BC:3000
.code

PUBLIC sgn_source_vehicle_detail_42b580
;RCX actual point, RDX captured camera.position, R8 actual camera forward9C,
;R9 actual lensB0, fifth argument pointer to actual61297C. No snapshot,
;no binary32/double spill before source comparisons; unordered masks retained.
sgn_source_vehicle_detail_42b580 PROC
    fld DWORD PTR [rcx]
    fsub DWORD PTR [rdx]
    fld DWORD PTR [rcx+4]
    fsub DWORD PTR [rdx+4]
    fld DWORD PTR [rcx+8]
    fsub DWORD PTR [rdx+8]
    fmul DWORD PTR [r8+8]
    fxch st(1)
    fmul DWORD PTR [r8+4]
    faddp st(1),st(0)
    fxch st(1)
    fmul DWORD PTR [r8]
    faddp st(1),st(0)
    fmul DWORD PTR [detail_scale]
    fdiv DWORD PTR [r9]
    fcom DWORD PTR [detail_mid]
    fnstsw ax
    test ah,41h
    jnz detail_low
    fcomp DWORD PTR [detail_far]
    fnstsw ax
    test ah,41h
    jnz detail_two
    mov eax,3
    jmp detail_quality
detail_two:
    mov eax,2
    jmp detail_quality
detail_low:
    fcomp DWORD PTR [detail_close]
    fnstsw ax
    test ah,41h
    jnz detail_zero
    mov eax,1
    jmp detail_quality
detail_zero:
    xor eax,eax
detail_quality:
    mov r10,QWORD PTR [rsp+40]
    mov ecx,DWORD PTR [r10]
    cmp ecx,19h
    jnc detail_quality25
    add eax,2
    jmp detail_limit
detail_quality25:
    cmp ecx,32h
    jnc detail_limit
    inc eax
detail_limit:
    cmp eax,3
    jbe detail_done
    mov eax,3
detail_done:
    ret
sgn_source_vehicle_detail_42b580 ENDP

PUBLIC sgn_source_vehicle_shadow_pose_41c44c
;RCX original80-byte local pose, RDX pointers to actual shadow position,
;forward,normal,scale392C. Preserve MOV/FPU interleaving, including three
;fresh scale reads while forward.X stays in ST0. All source W words copied.
sgn_source_vehicle_shadow_pose_41c44c PROC
    mov r8,QWORD PTR [rdx]
    mov eax,DWORD PTR [r8]
    mov DWORD PTR [rcx],eax
    mov eax,DWORD PTR [r8+4]
    mov DWORD PTR [rcx+4],eax
    mov eax,DWORD PTR [r8+8]
    mov DWORD PTR [rcx+8],eax
    mov r9,QWORD PTR [rdx+8]
    mov eax,DWORD PTR [r8+12]
    mov DWORD PTR [rcx+12],eax
    mov eax,DWORD PTR [r9]
    mov DWORD PTR [rcx+32],eax
    mov eax,DWORD PTR [r9+4]
    mov DWORD PTR [rcx+36],eax
    mov eax,DWORD PTR [r9+8]
    mov DWORD PTR [rcx+40],eax
    mov r8,QWORD PTR [rdx+16]
    mov eax,DWORD PTR [r9+12]
    mov DWORD PTR [rcx+44],eax
    mov eax,DWORD PTR [r8]
    mov DWORD PTR [rcx+48],eax
    mov eax,DWORD PTR [r8+4]
    fld DWORD PTR [rcx+32]
    mov DWORD PTR [rcx+52],eax
    mov eax,DWORD PTR [r8+8]
    fadd DWORD PTR [rcx]
    mov DWORD PTR [rcx+56],eax
    mov eax,DWORD PTR [r8+12]
    mov DWORD PTR [rcx+60],eax
    mov r9,QWORD PTR [rdx+24]
    mov eax,DWORD PTR [r9]
    mov DWORD PTR [rcx+16],eax
    mov eax,DWORD PTR [r9]
    mov DWORD PTR [rcx+20],eax
    mov eax,DWORD PTR [r9]
    fstp DWORD PTR [rcx+32]
    fld DWORD PTR [rcx+36]
    fadd DWORD PTR [rcx+4]
    mov DWORD PTR [rcx+24],eax
    fstp DWORD PTR [rcx+36]
    fld DWORD PTR [rcx+40]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+40]
    ret
sgn_source_vehicle_shadow_pose_41c44c ENDP
END
