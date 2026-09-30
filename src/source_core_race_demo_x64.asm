option casemap:none
; R17 source40DDD0 x87 suffix. Pinned .rdata operands:
;46E2F4 = 3EB33333 (0.35f), 46E1EC = 00000000 (0.0f).
.const
demo_46e2f4 DWORD 03eb33333h
demo_46e1ec DWORD 000000000h
.code
; RCX = actual velocity owner473A28, RDX = actual offset owner4D5B18.
; Same instruction order and operands as40DDEA..40DE2F; the FCOMP compares
; the unrounded ST0 sum (FST already stored the binary32 offset). TEST AH,1
; (C0) also takes the unordered edge, exactly as the source JZ.
PUBLIC sgn_source_demo_motion_40ddea
sgn_source_demo_motion_40ddea PROC
    fld DWORD PTR [rcx] ;0040ddea
    fsub DWORD PTR [demo_46e2f4] ;0040ddf0
    fstp DWORD PTR [rcx] ;0040ddf6
    fld DWORD PTR [rdx] ;0040ddfc
    fadd DWORD PTR [rcx] ;0040de02
    fst DWORD PTR [rdx] ;0040de08
    fcomp DWORD PTR [demo_46e1ec] ;0040de0e
    fnstsw ax ;0040de14
    test ah,1 ;0040de16
    jz demo_0040de2f ;0040de19
    mov DWORD PTR [rdx],0 ;0040de1b
    mov DWORD PTR [rcx],040a00000h ;0040de25
demo_0040de2f:
    ret ;0040de2f
sgn_source_demo_motion_40ddea ENDP
END
