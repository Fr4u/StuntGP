option casemap:none
.code
PUBLIC sgn_ring_x87_capture
sgn_ring_x87_capture PROC
    fnsave [rcx]
    frstor [rcx]
    ret
sgn_ring_x87_capture ENDP
PUBLIC sgn_ring_x87_restore
sgn_ring_x87_restore PROC
    frstor [rcx]
    ret
sgn_ring_x87_restore ENDP
PUBLIC sgn_ring_x87_step
sgn_ring_x87_step PROC FRAME
    sub rsp,136
    .allocstack 136
    .endprolog
    mov r10,rcx
    mov QWORD PTR [rsp+112],r8
    fnsave [rsp]
    frstor [r10]
    cmp edx,0
    je x87_0
    cmp edx,1
    je x87_1
    cmp edx,2
    je x87_2
    cmp edx,3
    je x87_3
    cmp edx,4
    je x87_4
    cmp edx,5
    je x87_5
    cmp edx,6
    je x87_6
    cmp edx,7
    je x87_7
    cmp edx,8
    je x87_8
    cmp edx,9
    je x87_9
    cmp edx,10
    je x87_10
    cmp edx,11
    je x87_11
    cmp edx,12
    je x87_12
    cmp edx,13
    je x87_13
    cmp edx,14
    je x87_14
    cmp edx,15
    je x87_15
    cmp edx,16
    je x87_16
    cmp edx,17
    je x87_17
    cmp edx,18
    je x87_18
    cmp edx,19
    je x87_19
    cmp edx,20
    je x87_20
    cmp edx,21
    je x87_21
    cmp edx,22
    je x87_22
    cmp edx,23
    je x87_23
    cmp edx,24
    je x87_24
    cmp edx,25
    je x87_25
    cmp edx,26
    je x87_26
    cmp edx,27
    je x87_27
    int 3 ;closed private enum only
x87_0:
    fld DWORD PTR [rsp+112]
    jmp x87_done
x87_1:
    fst DWORD PTR [rsp+112]
    jmp x87_done
x87_2:
    fstp DWORD PTR [rsp+112]
    jmp x87_done
x87_3:
    fadd DWORD PTR [rsp+112]
    jmp x87_done
x87_4:
    fsub DWORD PTR [rsp+112]
    jmp x87_done
x87_5:
    fmul DWORD PTR [rsp+112]
    jmp x87_done
x87_6:
    fdivr DWORD PTR [rsp+112]
    jmp x87_done
x87_7:
    fcom DWORD PTR [rsp+112]
    jmp x87_done
x87_8:
    fcomp DWORD PTR [rsp+112]
    jmp x87_done
x87_9:
    fld st(0)
    jmp x87_done
x87_10:
    fld st(2)
    jmp x87_done
x87_11:
    fld st(3)
    jmp x87_done
x87_12:
    fmul st(0),st(1)
    jmp x87_done
x87_13:
    fmul st(0),st(3)
    jmp x87_done
x87_14:
    fmul st(0),st(4)
    jmp x87_done
x87_15:
    faddp st(1),st(0)
    jmp x87_done
x87_16:
    fstp st(3)
    jmp x87_done
x87_17:
    fstp st(0)
    jmp x87_done
x87_18:
    fsqrt
    jmp x87_done
x87_19:
    fchs
    jmp x87_done
x87_20:
    fabs
    jmp x87_done
x87_21:
    fsub st(0),st(1)
    jmp x87_done
x87_22:
    fdiv st(0),st(1)
    jmp x87_done
x87_23:
    fxch st(1)
    jmp x87_done
x87_24:
    fnstsw WORD PTR [rsp+112]
    jmp x87_done
x87_25:
    fstcw WORD PTR [rsp+112]
    fwait
    jmp x87_done
x87_26:
    fldcw WORD PTR [rsp+112]
    jmp x87_done
x87_27:
    fistp QWORD PTR [rsp+112]
    jmp x87_done
x87_done:
    fnsave [r10]
    frstor [rsp]
    mov rax,QWORD PTR [rsp+112]
    add rsp,136
    ret
sgn_ring_x87_step ENDP

; R19 perf_entities: native forms of the same 28 closed ops (see source_ring_x87.hpp).
; Leaf procedures: RCX = QWORD operand, kept in the caller's home slot [rsp+8];
; the same single instruction as sgn_ring_x87_step's case; RAX = final slot QWORD.
PUBLIC sgn_ring_x87n_fld32
sgn_ring_x87n_fld32 PROC
    mov QWORD PTR [rsp+8],rcx
    fld DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fld32 ENDP
PUBLIC sgn_ring_x87n_fst32
sgn_ring_x87n_fst32 PROC
    mov QWORD PTR [rsp+8],rcx
    fst DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fst32 ENDP
PUBLIC sgn_ring_x87n_fstp32
sgn_ring_x87n_fstp32 PROC
    mov QWORD PTR [rsp+8],rcx
    fstp DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fstp32 ENDP
PUBLIC sgn_ring_x87n_fadd32
sgn_ring_x87n_fadd32 PROC
    mov QWORD PTR [rsp+8],rcx
    fadd DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fadd32 ENDP
PUBLIC sgn_ring_x87n_fsub32
sgn_ring_x87n_fsub32 PROC
    mov QWORD PTR [rsp+8],rcx
    fsub DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fsub32 ENDP
PUBLIC sgn_ring_x87n_fmul32
sgn_ring_x87n_fmul32 PROC
    mov QWORD PTR [rsp+8],rcx
    fmul DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fmul32 ENDP
PUBLIC sgn_ring_x87n_fdivr32
sgn_ring_x87n_fdivr32 PROC
    mov QWORD PTR [rsp+8],rcx
    fdivr DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fdivr32 ENDP
PUBLIC sgn_ring_x87n_fcom32
sgn_ring_x87n_fcom32 PROC
    mov QWORD PTR [rsp+8],rcx
    fcom DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fcom32 ENDP
PUBLIC sgn_ring_x87n_fcomp32
sgn_ring_x87n_fcomp32 PROC
    mov QWORD PTR [rsp+8],rcx
    fcomp DWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fcomp32 ENDP
PUBLIC sgn_ring_x87n_fld0
sgn_ring_x87n_fld0 PROC
    mov QWORD PTR [rsp+8],rcx
    fld st(0)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fld0 ENDP
PUBLIC sgn_ring_x87n_fld2
sgn_ring_x87n_fld2 PROC
    mov QWORD PTR [rsp+8],rcx
    fld st(2)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fld2 ENDP
PUBLIC sgn_ring_x87n_fld3
sgn_ring_x87n_fld3 PROC
    mov QWORD PTR [rsp+8],rcx
    fld st(3)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fld3 ENDP
PUBLIC sgn_ring_x87n_mul1
sgn_ring_x87n_mul1 PROC
    mov QWORD PTR [rsp+8],rcx
    fmul st(0),st(1)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_mul1 ENDP
PUBLIC sgn_ring_x87n_mul3
sgn_ring_x87n_mul3 PROC
    mov QWORD PTR [rsp+8],rcx
    fmul st(0),st(3)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_mul3 ENDP
PUBLIC sgn_ring_x87n_mul4
sgn_ring_x87n_mul4 PROC
    mov QWORD PTR [rsp+8],rcx
    fmul st(0),st(4)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_mul4 ENDP
PUBLIC sgn_ring_x87n_addp
sgn_ring_x87n_addp PROC
    mov QWORD PTR [rsp+8],rcx
    faddp st(1),st(0)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_addp ENDP
PUBLIC sgn_ring_x87n_pop3
sgn_ring_x87n_pop3 PROC
    mov QWORD PTR [rsp+8],rcx
    fstp st(3)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_pop3 ENDP
PUBLIC sgn_ring_x87n_pop0
sgn_ring_x87n_pop0 PROC
    mov QWORD PTR [rsp+8],rcx
    fstp st(0)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_pop0 ENDP
PUBLIC sgn_ring_x87n_sqrt
sgn_ring_x87n_sqrt PROC
    mov QWORD PTR [rsp+8],rcx
    fsqrt
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_sqrt ENDP
PUBLIC sgn_ring_x87n_chs
sgn_ring_x87n_chs PROC
    mov QWORD PTR [rsp+8],rcx
    fchs
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_chs ENDP
PUBLIC sgn_ring_x87n_abs
sgn_ring_x87n_abs PROC
    mov QWORD PTR [rsp+8],rcx
    fabs
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_abs ENDP
PUBLIC sgn_ring_x87n_sub1
sgn_ring_x87n_sub1 PROC
    mov QWORD PTR [rsp+8],rcx
    fsub st(0),st(1)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_sub1 ENDP
PUBLIC sgn_ring_x87n_div1
sgn_ring_x87n_div1 PROC
    mov QWORD PTR [rsp+8],rcx
    fdiv st(0),st(1)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_div1 ENDP
PUBLIC sgn_ring_x87n_exchange
sgn_ring_x87n_exchange PROC
    mov QWORD PTR [rsp+8],rcx
    fxch st(1)
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_exchange ENDP
PUBLIC sgn_ring_x87n_sw
sgn_ring_x87n_sw PROC
    mov QWORD PTR [rsp+8],rcx
    fnstsw WORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_sw ENDP
PUBLIC sgn_ring_x87n_cw
sgn_ring_x87n_cw PROC
    mov QWORD PTR [rsp+8],rcx
    fstcw WORD PTR [rsp+8]
    fwait
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_cw ENDP
PUBLIC sgn_ring_x87n_load_cw
sgn_ring_x87n_load_cw PROC
    mov QWORD PTR [rsp+8],rcx
    fldcw WORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_load_cw ENDP
PUBLIC sgn_ring_x87n_fistp64
sgn_ring_x87n_fistp64 PROC
    mov QWORD PTR [rsp+8],rcx
    fistp QWORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+8]
    ret
sgn_ring_x87n_fistp64 ENDP
END
