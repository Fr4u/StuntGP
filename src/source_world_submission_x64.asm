; Actual source data/store order. RCX output, RDX attribute, R8 vertex,
; R9 captured U/V pointer. No native packet or motion representation.
; All procedures are leaf functions using volatile registers, no stack writes.
option casemap:none
.code
PUBLIC sgn_source_submit_sky_4333e9
sgn_source_submit_sky_4333e9 PROC
    mov eax,DWORD PTR [r8] ;4333E9
    mov DWORD PTR [rcx],eax ;4333EB
    mov eax,DWORD PTR [r8+4] ;4333EE
    mov DWORD PTR [rcx+4],eax ;4333F1
    mov eax,DWORD PTR [r8+28] ;4333F4
    mov DWORD PTR [rcx+8],eax ;4333F9
    mov DWORD PTR [rcx+12],eax ;4333FC
    fld DWORD PTR [r9] ;433402, sky U comes first
    fadd DWORD PTR [rdx] ;433405
    fstp DWORD PTR [rcx+24] ;433407
    fld DWORD PTR [rdx+4] ;43340A
    fadd DWORD PTR [r9+4] ;43340D
    fstp DWORD PTR [rcx+28] ;433413
    mov DWORD PTR [rcx+16],0ffffffffh ;433416
    mov DWORD PTR [rcx+20],0 ;43341D
    ret
sgn_source_submit_sky_4333e9 ENDP

PUBLIC sgn_source_submit_opaque_4335e8
sgn_source_submit_opaque_4335e8 PROC
    fld DWORD PTR [rdx] ;4335E8, before XY/depth stores
    mov eax,DWORD PTR [r8] ;4335EA
    mov DWORD PTR [rcx],eax ;4335EC
    mov eax,DWORD PTR [r8+4] ;4335EF
    mov DWORD PTR [rcx+4],eax ;4335F2
    mov eax,DWORD PTR [r8+28] ;4335F5
    mov DWORD PTR [rcx+8],eax ;4335FA
    mov DWORD PTR [rcx+12],eax ;4335FD
    fadd DWORD PTR [r9] ;433603
    fstp DWORD PTR [rcx+24] ;433606
    fld DWORD PTR [rdx+4] ;433609
    fadd DWORD PTR [r9+4] ;43360C
    mov eax,DWORD PTR [rdx+12] ;43360F
    or eax,0ff000000h ;433612
    fstp DWORD PTR [rcx+28] ;433617
    mov DWORD PTR [rcx+16],eax ;43361A
    mov DWORD PTR [rcx+20],0 ;433620
    ret
sgn_source_submit_opaque_4335e8 ENDP

PUBLIC sgn_source_submit_specular_4337fc
sgn_source_submit_specular_4337fc PROC
    fld DWORD PTR [rdx] ;4337FC
    mov eax,DWORD PTR [r8] ;4337FE
    mov DWORD PTR [rcx],eax ;433800
    mov eax,DWORD PTR [r8+4] ;433803
    mov DWORD PTR [rcx+4],eax ;433806
    mov eax,DWORD PTR [r8+28] ;433809
    mov DWORD PTR [rcx+8],eax ;43380E
    mov DWORD PTR [rcx+12],eax ;433811
    fadd DWORD PTR [r9] ;433817
    fstp DWORD PTR [rcx+24] ;43381A
    fld DWORD PTR [rdx+4] ;43381D
    fadd DWORD PTR [r9+4] ;433820
    mov eax,DWORD PTR [rdx+12] ;433823
    fstp DWORD PTR [rcx+28] ;433826
    mov DWORD PTR [rcx+16],eax ;433829
    shr eax,24 ;43382C
    mov r10d,eax ;43382F
    shl r10d,8 ;433831
    or r10d,eax ;433834
    shl r10d,8 ;433836
    or r10d,eax ;433839
    mov DWORD PTR [rcx+20],r10d ;43383E
    ret
sgn_source_submit_specular_4337fc ENDP

PUBLIC sgn_source_submit_multi_433dd3
sgn_source_submit_multi_433dd3 PROC
    mov eax,DWORD PTR [r8] ;433DD3
    mov DWORD PTR [rcx],eax ;433DD5
    mov eax,DWORD PTR [r8+4] ;433DD8
    mov DWORD PTR [rcx+4],eax ;433DDB
    mov eax,DWORD PTR [r8+28] ;433DDE
    mov DWORD PTR [rcx+8],eax ;433DE1
    mov DWORD PTR [rcx+12],eax ;433DE4
    mov eax,DWORD PTR [rdx] ;433DE7
    mov DWORD PTR [rcx+24],eax ;433DE9
    mov eax,DWORD PTR [rdx+4] ;433DEC
    mov r10d,DWORD PTR [rdx+12] ;433DEF
    mov DWORD PTR [rcx+28],eax ;433DF2
    mov eax,DWORD PTR [r8+20] ;433DF5
    mov r11d,DWORD PTR [r8+24] ;433DF8
    mov DWORD PTR [rcx+32],eax ;433DFB
    mov DWORD PTR [rcx+36],r11d ;433DFE
    mov DWORD PTR [rcx+16],r10d ;433E01
    shr r10d,24 ;433E04
    mov eax,r10d ;433E07
    shl eax,8 ;433E09
    or eax,r10d ;433E0C
    shl eax,8 ;433E0E
    or eax,r10d ;433E11
    mov DWORD PTR [rcx+20],eax ;433E13
    ret
sgn_source_submit_multi_433dd3 ENDP

PUBLIC sgn_source_submit_alpha_433b07
sgn_source_submit_alpha_433b07 PROC
    fld DWORD PTR [rdx] ;433B07
    mov eax,DWORD PTR [r8] ;433B09
    mov DWORD PTR [rcx],eax ;433B0B
    mov eax,DWORD PTR [r8+4] ;433B0E
    mov DWORD PTR [rcx+4],eax ;433B11
    mov eax,DWORD PTR [r8+28] ;433B14
    mov DWORD PTR [rcx+8],eax ;433B19
    mov DWORD PTR [rcx+12],eax ;433B1B
    fadd DWORD PTR [r9] ;433B21
    fstp DWORD PTR [rcx+24] ;433B24
    fld DWORD PTR [rdx+4] ;433B27
    fadd DWORD PTR [r9+4] ;433B2A
    cmp DWORD PTR [rsp+40],050h ;433B2D, captured mode fifth argument
    fstp DWORD PTR [rcx+28] ;433B31
    mov DWORD PTR [rcx+20],0 ;433B34
    je alpha_vertex_color ;433B3B
    mov eax,DWORD PTR [rdx+12] ;433B3D
    and eax,0ffffffh ;433B40
    or eax,DWORD PTR [rsp+48] ;433B46, captured header alpha sixth argument
    mov DWORD PTR [rcx+16],eax ;433B48
    ret
alpha_vertex_color:
    mov eax,DWORD PTR [rdx+12] ;433B4D
    mov DWORD PTR [rcx+16],eax ;433B50
    ret
sgn_source_submit_alpha_433b07 ENDP
END
