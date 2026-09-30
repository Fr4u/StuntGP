OPTION CASEMAP:NONE
.code
; RCX=actual4864E8; RDX=20 DWORD destination addresses; R8=App name, R9D=extent.
; Returns scanned byte count inclNUL, or0 at first unrepresented source name read.
; Source FLD precedes REP MOVSD; all20 copied words precede each XYZ addition.
PUBLIC sgn_cold_template_40a3e9
sgn_cold_template_40a3e9 PROC
    fld DWORD PTR [rcx]
    xor r10d,r10d
cold_template_copy:
    mov eax,DWORD PTR [rcx+r10*4]
    mov r11,QWORD PTR [rdx+r10*8]
    mov DWORD PTR [r11],eax
    inc r10d
    cmp r10d,20
    jb cold_template_copy
    fadd DWORD PTR [rcx+32]
    mov r11,QWORD PTR [rdx+64]
    fstp DWORD PTR [r11]
    mov r11,QWORD PTR [rdx+8]
    fld DWORD PTR [r11]
    mov r11,QWORD PTR [rdx+72]
    fadd DWORD PTR [r11]
    fstp DWORD PTR [r11]
    mov r11,QWORD PTR [rdx+16]
    fld DWORD PTR [r11]
    xor r10d,r10d
cold_name_scan:
    cmp r10d,r9d
    jae cold_name_boundary
    cmp BYTE PTR [r8+r10],0
    lea r10d,[r10+1]
    jne cold_name_scan
    mov r11,QWORD PTR [rdx+80]
    fadd DWORD PTR [r11]
    fstp DWORD PTR [r11]
    mov eax,r10d
    ret
cold_name_boundary:
    fstp st(0) ;host exception transport only: no fabricated Z store on this boundary
    xor eax,eax
    ret
sgn_cold_template_40a3e9 ENDP

; Actual40A167..40A1C3. ST1 running total and ST0 step stay extended.
PUBLIC sgn_cold_torque_40a160
sgn_cold_torque_40a160 PROC
    xor r10d,r10d
    fld DWORD PTR [cold_zero]
    fld DWORD PTR [cold_step]
cold_torque_loop:
    fld st(1)
    fsqrt
    cmp r10d,32
    fstp DWORD PTR [rcx]
    fld st(0)
    faddp st(2),st(0)
    jbe cold_torque_next
    cmp r10d,64
    jae cold_torque_late
    fsub QWORD PTR [cold_step_decrement]
    fcom DWORD PTR [cold_zero]
    fnstsw ax
    test ah,1
    jz cold_torque_late
    fstp st(0)
    fld DWORD PTR [cold_negative_step]
cold_torque_late:
    cmp r10d,76
    jbe cold_torque_next
    fsub DWORD PTR [cold_late_decrement]
cold_torque_next:
    inc r10d
    add rcx,4
    cmp r10d,128
    jb cold_torque_loop
    fstp st(0)
    fstp st(0)
    ret
sgn_cold_torque_40a160 ENDP

PUBLIC sgn_cold_turbo_40a71b
sgn_cold_turbo_40a71b PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    xor r10d,r10d
cold_turbo_loop:
    mov DWORD PTR [rsp],r10d
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    inc r10d
    add rcx,4
    cmp r10d,128
    fmul DWORD PTR [cold_inverse128]
    fstp DWORD PTR [rcx-4]
    jb cold_turbo_loop
    add rsp,24
    ret
sgn_cold_turbo_40a71b ENDP

PUBLIC sgn_cold_launch_40a78b
sgn_cold_launch_40a78b PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [cold_launch_scale]
    fstp DWORD PTR [rdx]
    ret
sgn_cold_launch_40a78b ENDP

PUBLIC sgn_cold_friction_40a82d
sgn_cold_friction_40a82d PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    xor r10d,r10d
cold_friction_loop:
    mov DWORD PTR [rsp],r10d
    mov DWORD PTR [rsp+4],0
    fild QWORD PTR [rsp]
    fcomp DWORD PTR [cold_friction_limit]
    fnstsw ax
    test ah,1
    jz cold_friction_high
    mov DWORD PTR [rcx],03f800000h
    jmp cold_friction_next
cold_friction_high:
    mov DWORD PTR [rcx],03ecccccdh
cold_friction_next:
    inc r10d
    add rcx,4
    cmp r10d,128
    jb cold_friction_loop
    add rsp,24
    ret
sgn_cold_friction_40a82d ENDP
.const
; Exact pinned readonly PE literals, in PE-DATA.json.
cold_zero DWORD 0
cold_step DWORD 03cb66666h
cold_step_decrement QWORD 03f50000000000000h
cold_negative_step DWORD 0bb23d70ah
cold_late_decrement DWORD 0399d4952h
cold_inverse128 DWORD 03c000000h
cold_launch_scale DWORD 041c80000h
cold_friction_limit DWORD 0424ccccdh
END
