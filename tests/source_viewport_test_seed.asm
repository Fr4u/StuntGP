; Receiving-only environment seed. No FXRSTOR/XMM register clobber across ABI.
; FNSTENV temporary masking is intentional only here; production never uses it.
OPTION CASEMAP:NONE
.code
PUBLIC sgn_viewport_test_seed_fpu
sgn_viewport_test_seed_fpu PROC
    fnstenv [rsp+8]                    ;28 bytes fit caller's32-byte home space
    mov WORD PTR [rsp+8],cx
    mov WORD PTR [rsp+12],dx
    mov WORD PTR [rsp+16],r8w
    fldenv [rsp+8]
    ret
sgn_viewport_test_seed_fpu ENDP
END
