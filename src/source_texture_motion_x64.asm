option casemap:none
.const
uv_one DWORD 03f800000h
uv_minus_one DWORD 0bf800000h
uv_wave_factor DWORD 03d400000h
.code
PUBLIC sgn_source_uv_first_4258de
sgn_source_uv_first_4258de PROC
    fld DWORD PTR [rcx] ;4258DE actual table word
    fmul DWORD PTR [uv_wave_factor] ;4258E5
    mov eax,DWORD PTR [rdx] ;4258EB actual captured PMD kind
    cmp eax,-1 ;4258ED
    fstp DWORD PTR [r8] ;4258F0 actual577650, after kind read/compare
    ret
sgn_source_uv_first_4258de ENDP

PUBLIC sgn_source_uv_record_425904
sgn_source_uv_record_425904 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
uv_00425904:
    cmp r8d,02h ;00425904
uv_00425907:
    jnz uv_00425942 ;00425907
uv_00425909:
    fld DWORD PTR [rcx + 0ch] ;00425909
uv_0042590c:
    fadd DWORD PTR [rcx + 04h] ;0042590c
uv_0042590f:
    mov eax,[rdx] ;0042590f
uv_00425914:
    mov DWORD PTR [rcx + 08h],eax ;00425914
uv_00425917:
    fst DWORD PTR [rsp] ;00425917
uv_0042591a:
    fstp DWORD PTR [rcx + 04h] ;0042591a
uv_0042591d:
    fld DWORD PTR [rsp] ;0042591d
uv_00425920:
    fcomp DWORD PTR [uv_one] ;00425920
uv_00425926:
    fnstsw ax ;00425926
uv_00425928:
    test ah,041h ;00425928
uv_0042592b:
    jnz uv_004259ca ;0042592b
uv_00425931:
    fld DWORD PTR [rsp] ;00425931
uv_00425934:
    fsub DWORD PTR [uv_one] ;00425934
uv_0042593a:
    fstp DWORD PTR [rcx + 04h] ;0042593a
uv_0042593d:
    jmp uv_004259ca ;0042593d
uv_00425942:
    fld DWORD PTR [rcx + 04h] ;00425942
uv_00425945:
    fadd DWORD PTR [rcx + 0ch] ;00425945
uv_00425948:
    fst DWORD PTR [rsp] ;00425948
uv_0042594b:
    fstp DWORD PTR [rcx + 04h] ;0042594b
uv_0042594e:
    fld DWORD PTR [rcx + 010h] ;0042594e
uv_00425951:
    fadd DWORD PTR [rcx + 08h] ;00425951
uv_00425954:
    fst DWORD PTR [rsp+4] ;00425954
uv_00425957:
    fstp DWORD PTR [rcx + 08h] ;00425957
uv_0042595a:
    fld DWORD PTR [rsp] ;0042595a
uv_0042595d:
    fcomp DWORD PTR [uv_one] ;0042595d
uv_00425963:
    fnstsw ax ;00425963
uv_00425965:
    test ah,041h ;00425965
uv_00425968:
    jnz uv_00425976 ;00425968
uv_0042596a:
    fld DWORD PTR [rsp] ;0042596a
uv_0042596d:
    fsub DWORD PTR [uv_one] ;0042596d
uv_00425973:
    fstp DWORD PTR [rcx + 04h] ;00425973
uv_00425976:
    fld DWORD PTR [rsp+4] ;00425976
uv_00425979:
    fcomp DWORD PTR [uv_one] ;00425979
uv_0042597f:
    fnstsw ax ;0042597f
uv_00425981:
    test ah,041h ;00425981
uv_00425984:
    jnz uv_00425992 ;00425984
uv_00425986:
    fld DWORD PTR [rsp+4] ;00425986
uv_00425989:
    fsub DWORD PTR [uv_one] ;00425989
uv_0042598f:
    fstp DWORD PTR [rcx + 08h] ;0042598f
uv_00425992:
    fld DWORD PTR [rcx + 04h] ;00425992
uv_00425995:
    fcomp DWORD PTR [uv_minus_one] ;00425995
uv_0042599b:
    fnstsw ax ;0042599b
uv_0042599d:
    test ah,01h ;0042599d
uv_004259a0:
    jz uv_004259ae ;004259a0
uv_004259a2:
    fld DWORD PTR [rcx + 04h] ;004259a2
uv_004259a5:
    fadd DWORD PTR [uv_one] ;004259a5
uv_004259ab:
    fstp DWORD PTR [rcx + 04h] ;004259ab
uv_004259ae:
    fld DWORD PTR [rcx + 08h] ;004259ae
uv_004259b1:
    fcomp DWORD PTR [uv_minus_one] ;004259b1
uv_004259b7:
    fnstsw ax ;004259b7
uv_004259b9:
    test ah,01h ;004259b9
uv_004259bc:
    jz uv_004259ca ;004259bc
uv_004259be:
    fld DWORD PTR [rcx + 08h] ;004259be
uv_004259c1:
    fadd DWORD PTR [uv_one] ;004259c1
uv_004259c7:
    fstp DWORD PTR [rcx + 08h] ;004259c7
uv_004259ca:
    add rsp,16
    ret
sgn_source_uv_record_425904 ENDP

PUBLIC sgn_source_uv_sky_4259e3
sgn_source_uv_sky_4259e3 PROC FRAME
    sub rsp,16
    .allocstack 16
    .endprolog
uv_004259e3:
    fld DWORD PTR [rcx + 08h] ;004259e3
uv_004259e6:
    fadd DWORD PTR [rcx] ;004259e6
uv_004259e8:
    fstp DWORD PTR [rcx] ;004259e8
uv_004259ea:
    fld DWORD PTR [rcx + 04h] ;004259ea
uv_004259ed:
    fadd DWORD PTR [rcx + 0ch] ;004259ed
uv_004259f0:
    fstp DWORD PTR [rcx + 04h] ;004259f0
uv_004259f3:
    fld DWORD PTR [rcx] ;004259f3
uv_004259f5:
    fcomp DWORD PTR [uv_one] ;004259f5
uv_004259fb:
    fnstsw ax ;004259fb
uv_004259fd:
    test ah,041h ;004259fd
uv_00425a00:
    jnz uv_00425a0c ;00425a00
uv_00425a02:
    fld DWORD PTR [rcx] ;00425a02
uv_00425a04:
    fsub DWORD PTR [uv_one] ;00425a04
uv_00425a0a:
    fstp DWORD PTR [rcx] ;00425a0a
uv_00425a0c:
    fld DWORD PTR [rcx + 04h] ;00425a0c
uv_00425a0f:
    fcomp DWORD PTR [uv_one] ;00425a0f
uv_00425a15:
    fnstsw ax ;00425a15
uv_00425a17:
    test ah,041h ;00425a17
uv_00425a1a:
    jnz uv_00425a28 ;00425a1a
uv_00425a1c:
    fld DWORD PTR [rcx + 04h] ;00425a1c
uv_00425a1f:
    fsub DWORD PTR [uv_one] ;00425a1f
uv_00425a25:
    fstp DWORD PTR [rcx + 04h] ;00425a25
uv_00425a28:
    fld DWORD PTR [rcx] ;00425a28
uv_00425a2a:
    fcomp DWORD PTR [uv_minus_one] ;00425a2a
uv_00425a30:
    fnstsw ax ;00425a30
uv_00425a32:
    test ah,01h ;00425a32
uv_00425a35:
    jz uv_00425a41 ;00425a35
uv_00425a37:
    fld DWORD PTR [rcx] ;00425a37
uv_00425a39:
    fadd DWORD PTR [uv_one] ;00425a39
uv_00425a3f:
    fstp DWORD PTR [rcx] ;00425a3f
uv_00425a41:
    fld DWORD PTR [rcx + 04h] ;00425a41
uv_00425a44:
    fcomp DWORD PTR [uv_minus_one] ;00425a44
uv_00425a4a:
    fnstsw ax ;00425a4a
uv_00425a4c:
    test ah,01h ;00425a4c
uv_00425a4f:
    jz uv_00425a5d ;00425a4f
uv_00425a51:
    fld DWORD PTR [rcx + 04h] ;00425a51
uv_00425a54:
    fadd DWORD PTR [uv_one] ;00425a54
uv_00425a5a:
    fstp DWORD PTR [rcx + 04h] ;00425a5a
uv_00425a5d:
    add rsp,16
    ret
sgn_source_uv_sky_4259e3 ENDP

END
