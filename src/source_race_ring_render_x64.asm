option casemap:none
.const
ring_zero DWORD 0 ;46E1EC
ring_one DWORD 03f800000h ;46E264
ring_hundred DWORD 042c80000h ;46E2F0
.code

; Actual462860. Caller supplies a balanced FPU stack with one argument.
ring_ftol MACRO
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

PUBLIC sgn_source_ring_nonzero_41f313
sgn_source_ring_nonzero_41f313 PROC
    fld DWORD PTR [rcx]
    fcomp DWORD PTR [ring_zero]
    fnstsw ax
    test ah,40h ;C3: unordered also skips the source head
    setz al
    movzx eax,al
    ret
sgn_source_ring_nonzero_41f313 ENDP

PUBLIC sgn_source_ring_depth_41ee19
sgn_source_ring_depth_41ee19 PROC
    fld DWORD PTR [rcx+8]
    fcomp DWORD PTR [ring_zero]
    fnstsw ax
    test ah,1
    jnz ring_depth_reject
    fld DWORD PTR [rcx+8]
    fcomp DWORD PTR [ring_one]
    fnstsw ax
    test ah,41h
    jz ring_depth_reject
    mov eax,1
    ret
ring_depth_reject:
    xor eax,eax
    ret
sgn_source_ring_depth_41ee19 ENDP

PUBLIC sgn_source_ring_mul_float
sgn_source_ring_mul_float PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fstp DWORD PTR [r8]
    ret
sgn_source_ring_mul_float ENDP

PUBLIC sgn_source_ring_mul_extended
sgn_source_ring_mul_extended PROC
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fstp TBYTE PTR [r8] ;lossless transport of the original retained x87 value
    ret
sgn_source_ring_mul_extended ENDP

PUBLIC sgn_source_ring_mul_integer
sgn_source_ring_mul_integer PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    fld DWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    ring_ftol
    add rsp,16
    ret
sgn_source_ring_mul_integer ENDP

PUBLIC sgn_source_ring_uint_alpha
sgn_source_ring_uint_alpha PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov eax,DWORD PTR [rcx]
    mov DWORD PTR [rsp],eax
    mov DWORD PTR [rsp+4],0 ;41F339, unsigned head alpha is zero-extended
    fild QWORD PTR [rsp]
    fmul DWORD PTR [rdx]
    ring_ftol
    add rsp,16
    ret
sgn_source_ring_uint_alpha ENDP

PUBLIC sgn_source_ring_world_point_41e792
sgn_source_ring_world_point_41e792 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov r8,QWORD PTR [rcx] ;actual shape point
    mov r9,QWORD PTR [rcx+8] ;actual current vehicle orientation
    mov r10,QWORD PTR [rcx+16] ;actual ring point
    mov r11,QWORD PTR [rcx+24] ;captured old/new width
    fld DWORD PTR [r11]
    fmul DWORD PTR [r8]
    fld DWORD PTR [r11]
    fmul DWORD PTR [r8+4]
    fld DWORD PTR [r11]
    fmul DWORD PTR [r8+8]
    fld st(2)
    fmul DWORD PTR [r9]
    fld st(1)
    fmul DWORD PTR [r9+20h]
    faddp st(1),st(0)
    fld st(2)
    fmul DWORD PTR [r9+10h]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp] ;41E7C5
    fld st(0)
    fmul DWORD PTR [r9+24h]
    fld st(2)
    fmul DWORD PTR [r9+14h]
    faddp st(1),st(0)
    fld st(3)
    fmul DWORD PTR [r9+4]
    faddp st(1),st(0)
    fstp DWORD PTR [rsp+4] ;41E7E4
    fxch st(2)
    fmul DWORD PTR [r9+8]
    fxch st(2)
    fmul DWORD PTR [r9+28h]
    faddp st(2),st(0)
    fmul DWORD PTR [r9+18h]
    faddp st(1),st(0)
    fld DWORD PTR [rsp]
    fadd DWORD PTR [r10]
    fstp DWORD PTR [rdx]
    fld DWORD PTR [rsp+4]
    fadd DWORD PTR [r10+4]
    fstp DWORD PTR [rdx+4]
    fadd DWORD PTR [r10+8]
    fstp DWORD PTR [rdx+8]
    ;The PE does NOT write the local fourth word. Do not initialize it here.
    add rsp,16
    ret
sgn_source_ring_world_point_41e792 ENDP

PUBLIC sgn_source_ring_screen_head_41ee41
sgn_source_ring_screen_head_41ee41 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
    mov r8,QWORD PTR [rcx] ;profile
    mov r9,QWORD PTR [rcx+8] ;actual ring point
    mov r10,QWORD PTR [rcx+16] ;source scale argument
    mov r11,QWORD PTR [rcx+24] ;logical source EBP, frame output
    fld DWORD PTR [r10]
    fmul DWORD PTR [r8+100h]
    fmul DWORD PTR [r9+8]
    fmul DWORD PTR [ring_hundred]
    fstp DWORD PTR [r11-4] ;41EEB8
    fld DWORD PTR [r10]
    fmul DWORD PTR [r8+104h]
    fmul DWORD PTR [r9+8]
    fmul DWORD PTR [ring_hundred]
    fstp DWORD PTR [rsp] ;41EF19, original local argument slot
    fld DWORD PTR [r9]
    fsub DWORD PTR [r11-4]
    fst DWORD PTR [r11-104h] ;41EF21, retains extended leftX
    fld DWORD PTR [r9+4]
    fsub DWORD PTR [rsp]
    fst DWORD PTR [r11-100h]
    fld DWORD PTR [r11-4]
    fadd DWORD PTR [r9]
    fst DWORD PTR [r11-4]
    fstp DWORD PTR [r11-0e4h]
    mov eax,DWORD PTR [r11-4]
    fstp DWORD PTR [r11-0e0h]
    fld DWORD PTR [rsp]
    fadd DWORD PTR [r9+4]
    mov DWORD PTR [r11-0c4h],eax
    fst DWORD PTR [rsp]
    fstp DWORD PTR [r11-0c0h]
    mov eax,DWORD PTR [rsp]
    fstp DWORD PTR [r11-0a4h]
    mov DWORD PTR [r11-0a0h],eax
    add rsp,16
    ret
sgn_source_ring_screen_head_41ee41 ENDP

 ;41EF90/44EDA0 now execute ordered source operations through
 ;source_ring_x87; no retired41F199 boundary cleanup remains here.
END
