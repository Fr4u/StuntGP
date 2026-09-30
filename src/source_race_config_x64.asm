OPTION CASEMAP:NONE
.code
;4349A2 and434AC2: the real4631A7 binary64 result is loaded then stored to its captured field.
PUBLIC sgn_source_vehicle_store_4349a2
sgn_source_vehicle_store_4349a2 PROC
    fld QWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
sgn_source_vehicle_store_4349a2 ENDP

;40A1DD..40A28F after the actual zero publication and valid curve-identity capture.
;RCX=maxRPM RDX=limit R8=maxTorque R9=selected curve; stack5=output,6=scale.
;No parameter copy or SSE replacement of unrounded FCOM operands.
PUBLIC sgn_source_config_engine_40a1d0
sgn_source_config_engine_40a1d0 PROC FRAME
    mov r10,QWORD PTR [rsp+40]
    mov r11,QWORD PTR [rsp+48]
    sub rsp,24
    .allocstack 24
    .endprolog
    mov QWORD PTR [rsp],0
    fld DWORD PTR [config_zero]
config_engine_loop:
    fild QWORD PTR [rsp]
    fmul DWORD PTR [rcx]
    fmul DWORD PTR [config_inverse128]
    fld st(0)
    fmul DWORD PTR [config_rpm_radians]
    fmul DWORD PTR [r9]
    fmul DWORD PTR [config_horsepower]
    fst DWORD PTR [rsp+8]
    fstp DWORD PTR [r10]
    fcomp DWORD PTR [rdx]
    fnstsw ax
    test ah,41h
    jz config_engine_next
    fld DWORD PTR [rsp+8]
    fcomp DWORD PTR [r11]
    fnstsw ax
    test ah,41h
    jnz config_engine_max_torque
    mov eax,DWORD PTR [rsp+8]
    mov DWORD PTR [r11],eax
config_engine_max_torque:
    fcom DWORD PTR [r9]
    fnstsw ax
    test ah,1
    jz config_engine_next
    fstp st(0)
    fld DWORD PTR [r9]
config_engine_next:
    inc DWORD PTR [rsp]
    add r10,4
    add r9,4
    cmp DWORD PTR [rsp],128
    jb config_engine_loop
    fstp st(0)
    fld DWORD PTR [r8]
    fmul DWORD PTR [r11]
    fstp DWORD PTR [r11]
    add rsp,24
    ret
sgn_source_config_engine_40a1d0 ENDP
.const
config_zero DWORD 0
config_inverse128 DWORD 03c000000h
config_rpm_radians DWORD 03dd67750h
config_horsepower DWORD 03aafb322h
END
