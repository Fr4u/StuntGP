option casemap:none
.const
audio_00477658 DWORD 0bf800000h ;pinned PE00477658
audio_0047765c DWORD 000000000h ;pinned PE0047765C
audio_00477660 DWORD 000000000h ;pinned PE00477660
audio_00477668 DWORD 000000000h ;pinned PE00477668
audio_0047766c DWORD 03f800000h ;pinned PE0047766C
audio_00477670 DWORD 000000000h ;pinned PE00477670
audio_00477678 DWORD 000000000h ;pinned PE00477678
audio_0047767c DWORD 000000000h ;pinned PE0047767C
audio_00477680 DWORD 0bf800000h ;pinned PE00477680
audio_00477688 DWORD 03f800000h ;pinned PE00477688
audio_0047768c DWORD 000000000h ;pinned PE0047768C
audio_00477690 DWORD 000000000h ;pinned PE00477690
audio_00477698 DWORD 000000000h ;pinned PE00477698
audio_0047769c DWORD 0bf800000h ;pinned PE0047769C
audio_004776a0 DWORD 000000000h ;pinned PE004776A0
audio_004776a8 DWORD 000000000h ;pinned PE004776A8
audio_004776ac DWORD 000000000h ;pinned PE004776AC
audio_004776b0 DWORD 03f800000h ;pinned PE004776B0
audio_004776d8 DWORD 0bf000000h ;pinned PE004776D8
audio_004776dc DWORD 03f000000h ;pinned PE004776DC
audio_004776e0 DWORD 0bf000000h ;pinned PE004776E0
audio_00477738 DWORD 03f000000h ;pinned PE00477738
audio_0047773c DWORD 0bf000000h ;pinned PE0047773C
audio_00477740 DWORD 03f000000h ;pinned PE00477740
.code
PUBLIC sgn_source_audio_contains_4278a0
sgn_source_audio_contains_4278a0 PROC FRAME
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp,0b8h
    .allocstack 0b8h
    .endprolog
    mov rsi,rcx ;actual region.position; original ECX
    mov rdi,rdx ;actual camera XYZ; original EDX
audio_at_004278af:
    fld DWORD PTR [rsi + 36] ;source 004278AF
audio_at_004278b2:
    fmul DWORD PTR [rsi + 56] ;source 004278B2
audio_at_004278b5:
    fld DWORD PTR [rsi + 52] ;source 004278B5
audio_at_004278b8:
    fmul DWORD PTR [rsi + 40] ;source 004278B8
audio_at_004278bb:
    lea rax,[rsi + 48] ;source 004278BB
audio_at_004278be:
    lea rdx,[rsi + 32] ;source 004278BE
audio_at_004278c1:
    lea rcx,[rsp+20] ;source 004278C1
audio_at_004278c7:
    fsubp st(1),st(0) ;source 004278C7
audio_at_004278c9:
audio_at_004278ca:
audio_at_004278cb:
    lea rcx,[rsp+148] ;source 004278CB
audio_at_004278ce:
    fstp DWORD PTR [rsp+148] ;source 004278CE
audio_at_004278d1:
    fld DWORD PTR [rax] ;source 004278D1
audio_at_004278d3:
    fmul DWORD PTR [rsi + 40] ;source 004278D3
audio_at_004278d6:
    fld DWORD PTR [rdx] ;source 004278D6
audio_at_004278d8:
    fmul DWORD PTR [rsi + 56] ;source 004278D8
audio_at_004278db:
    fsubp st(1),st(0) ;source 004278DB
audio_at_004278dd:
    fstp DWORD PTR [rsp+152] ;source 004278DD
audio_at_004278e0:
    fld DWORD PTR [rsi + 52] ;source 004278E0
audio_at_004278e3:
    fmul DWORD PTR [rdx] ;source 004278E3
audio_at_004278e5:
    fld DWORD PTR [rax] ;source 004278E5
audio_at_004278e7:
    fmul DWORD PTR [rsi + 36] ;source 004278E7
audio_at_004278ea:
    fsubp st(1),st(0) ;source 004278EA
audio_at_004278ec:
    fstp DWORD PTR [rsp+156] ;source 004278EC
audio_at_004278ef:
    mov DWORD PTR [rsp+160],0 ;NEW PORT-FIX-001: original EBP-18 direction W ;source 004278EF
    mov r8d,DWORD PTR [rsp+148] ;source 004278EF
    mov DWORD PTR [rsp+20],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsp+152] ;source 004278EF
    mov DWORD PTR [rsp+24],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsp+156] ;source 004278EF
    mov DWORD PTR [rsp+28],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsp+160] ;source 004278EF
    mov DWORD PTR [rsp+32],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+32] ;source 004278EF
    mov DWORD PTR [rsp+36],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+36] ;source 004278EF
    mov DWORD PTR [rsp+40],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+40] ;source 004278EF
    mov DWORD PTR [rsp+44],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+44] ;source 004278EF
    mov DWORD PTR [rsp+48],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+48] ;source 004278EF
    mov DWORD PTR [rsp+52],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+52] ;source 004278EF
    mov DWORD PTR [rsp+56],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+56] ;source 004278EF
    mov DWORD PTR [rsp+60],r8d ;source 004278EF
    mov r8d,DWORD PTR [rsi+60] ;source 004278EF
    mov DWORD PTR [rsp+64],r8d ;source 004278EF
    mov DWORD PTR [rsp+68],0 ;source 004278EF
    mov DWORD PTR [rsp+72],0 ;source 004278EF
    mov DWORD PTR [rsp+76],0 ;source 004278EF
    mov DWORD PTR [rsp+80],3f800000h ;source 004278EF
audio_at_004278f4:
    lea rdx,[rsp+20] ;source 004278F4
audio_at_004278fa:
    lea rcx,[rsp+84] ;source 004278FA
audio_at_004278fd:
    mov r8d,DWORD PTR [rsp+20] ;source 004278FD
    mov DWORD PTR [rsp+84],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+36] ;source 004278FD
    mov DWORD PTR [rsp+88],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+52] ;source 004278FD
    mov DWORD PTR [rsp+92],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+68] ;source 004278FD
    mov DWORD PTR [rsp+96],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+24] ;source 004278FD
    mov DWORD PTR [rsp+100],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+40] ;source 004278FD
    mov DWORD PTR [rsp+104],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+56] ;source 004278FD
    mov DWORD PTR [rsp+108],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+72] ;source 004278FD
    mov DWORD PTR [rsp+112],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+28] ;source 004278FD
    mov DWORD PTR [rsp+116],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+44] ;source 004278FD
    mov DWORD PTR [rsp+120],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+60] ;source 004278FD
    mov DWORD PTR [rsp+124],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+76] ;source 004278FD
    mov DWORD PTR [rsp+128],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+32] ;source 004278FD
    mov DWORD PTR [rsp+132],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+48] ;source 004278FD
    mov DWORD PTR [rsp+136],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+64] ;source 004278FD
    mov DWORD PTR [rsp+140],r8d ;source 004278FD
    mov r8d,DWORD PTR [rsp+80] ;source 004278FD
    mov DWORD PTR [rsp+144],r8d ;source 004278FD
audio_at_00427902:
    fld DWORD PTR [rdi] ;source 00427902
audio_at_00427904:
    fsub DWORD PTR [rsi] ;source 00427904
audio_at_00427906:
    fld DWORD PTR [rdi + 4] ;source 00427906
audio_at_00427909:
    fsub DWORD PTR [rsi + 4] ;source 00427909
audio_at_0042790c:
    fld DWORD PTR [rdi + 8] ;source 0042790C
audio_at_0042790f:
    fsub DWORD PTR [rsi + 8] ;source 0042790F
audio_at_00427912:
    fld DWORD PTR [rsp+116] ;source 00427912
audio_at_00427915:
    fmul st(0),st(1) ;source 00427915
audio_at_00427917:
    fld DWORD PTR [rsp+100] ;source 00427917
audio_at_0042791a:
    fmul st(0),st(3) ;source 0042791A
audio_at_0042791c:
    faddp st(1),st(0) ;source 0042791C
audio_at_0042791e:
    fld DWORD PTR [rsp+84] ;source 0042791E
audio_at_00427921:
    fmul st(0),st(4) ;source 00427921
audio_at_00427923:
    faddp st(1),st(0) ;source 00427923
audio_at_00427925:
    fadd DWORD PTR [rsp+132] ;source 00427925
audio_at_00427928:
    fstp DWORD PTR [rsp+148] ;source 00427928
audio_at_0042792b:
    fld DWORD PTR [rsp+120] ;source 0042792B
audio_at_0042792e:
    fmul st(0),st(1) ;source 0042792E
audio_at_00427930:
    fld DWORD PTR [rsp+104] ;source 00427930
audio_at_00427933:
    fmul st(0),st(3) ;source 00427933
audio_at_00427935:
    faddp st(1),st(0) ;source 00427935
audio_at_00427937:
    fld DWORD PTR [rsp+88] ;source 00427937
audio_at_0042793a:
    fmul st(0),st(4) ;source 0042793A
audio_at_0042793c:
    faddp st(1),st(0) ;source 0042793C
audio_at_0042793e:
    fadd DWORD PTR [rsp+136] ;source 0042793E
audio_at_00427941:
    fstp DWORD PTR [rsp+152] ;source 00427941
audio_at_00427944:
    fld DWORD PTR [rsp+124] ;source 00427944
audio_at_00427947:
    fmul st(0),st(1) ;source 00427947
audio_at_00427949:
    fld DWORD PTR [rsp+108] ;source 00427949
audio_at_0042794c:
    fmul st(0),st(3) ;source 0042794C
audio_at_0042794e:
    faddp st(1),st(0) ;source 0042794E
audio_at_00427950:
    fld DWORD PTR [rsp+92] ;source 00427950
audio_at_00427953:
    fmul st(0),st(4) ;source 00427953
audio_at_00427955:
    faddp st(1),st(0) ;source 00427955
audio_at_00427957:
    fadd DWORD PTR [rsp+140] ;source 00427957
audio_at_0042795a:
    fstp st(3) ;source 0042795A
audio_at_0042795c:
    fstp st(0) ;source 0042795C
audio_at_0042795e:
    fstp st(0) ;source 0042795E
audio_at_00427960:
    fld DWORD PTR [rsp+148] ;source 00427960
audio_at_00427963:
    fadd DWORD PTR [rsi] ;source 00427963
audio_at_00427965:
    fstp DWORD PTR [rsp+148] ;source 00427965
audio_at_00427968:
    fld DWORD PTR [rsp+152] ;source 00427968
audio_at_0042796b:
    fadd DWORD PTR [rsi + 4] ;source 0042796B
audio_at_0042796e:
    fstp DWORD PTR [rsp+152] ;source 0042796E
audio_at_00427971:
    fadd DWORD PTR [rsi + 8] ;source 00427971
audio_at_00427974:
    fstp DWORD PTR [rsp+156] ;source 00427974
audio_at_00427977:
    fld DWORD PTR [audio_004776d8] ;source 00427977
audio_at_0042797d:
    fmul DWORD PTR [rsi + 16] ;source 0042797D
audio_at_00427980:
    fld DWORD PTR [audio_004776dc] ;source 00427980
audio_at_00427986:
    fmul DWORD PTR [rsi + 20] ;source 00427986
audio_at_00427989:
    fstp DWORD PTR [rsp+168] ;source 00427989
audio_at_0042798c:
    fld DWORD PTR [audio_004776e0] ;source 0042798C
audio_at_00427992:
    fmul DWORD PTR [rsi + 24] ;source 00427992
audio_at_00427995:
    fstp DWORD PTR [rsp+172] ;source 00427995
audio_at_00427998:
    fadd DWORD PTR [rsi] ;source 00427998
audio_at_0042799a:
    fld DWORD PTR [rsp+168] ;source 0042799A
audio_at_0042799d:
    fadd DWORD PTR [rsi + 4] ;source 0042799D
audio_at_004279a0:
    fstp DWORD PTR [rsp+168] ;source 004279A0
audio_at_004279a3:
    fld DWORD PTR [rsp+172] ;source 004279A3
audio_at_004279a6:
    fadd DWORD PTR [rsi + 8] ;source 004279A6
audio_at_004279a9:
    fstp DWORD PTR [rsp+172] ;source 004279A9
audio_at_004279ac:
    fld DWORD PTR [rsp+148] ;source 004279AC
audio_at_004279af:
    fsub st(0),st(1) ;source 004279AF
audio_at_004279b1:
    fstp DWORD PTR [rsp+164] ;source 004279B1
audio_at_004279b4:
    fstp st(0) ;source 004279B4
audio_at_004279b6:
    fld DWORD PTR [rsp+152] ;source 004279B6
audio_at_004279b9:
    fsub DWORD PTR [rsp+168] ;source 004279B9
audio_at_004279bc:
    fstp DWORD PTR [rsp+168] ;source 004279BC
audio_at_004279bf:
    fld DWORD PTR [rsp+156] ;source 004279BF
audio_at_004279c2:
    fsub DWORD PTR [rsp+172] ;source 004279C2
audio_at_004279c5:
    fstp DWORD PTR [rsp+172] ;source 004279C5
audio_at_004279c8:
    fld DWORD PTR [audio_00477660] ;source 004279C8
audio_at_004279ce:
    fmul DWORD PTR [rsp+172] ;source 004279CE
audio_at_004279d1:
    fld DWORD PTR [audio_0047765c] ;source 004279D1
audio_at_004279d7:
    fmul DWORD PTR [rsp+168] ;source 004279D7
audio_at_004279da:
    faddp st(1),st(0) ;source 004279DA
audio_at_004279dc:
    fld DWORD PTR [audio_00477658] ;source 004279DC
audio_at_004279e2:
    fmul DWORD PTR [rsp+164] ;source 004279E2
audio_at_004279e5:
    faddp st(1),st(0) ;source 004279E5
audio_at_004279e7:
    fstp DWORD PTR [rsp+180] ;source 004279E7
audio_at_004279ea:
    mov eax,DWORD PTR [rsp+180] ;source 004279EA
audio_at_004279ed:
    test eax,eax ;source 004279ED
audio_at_004279ef:
    jl audio_at_004279f9 ;source 004279EF
audio_at_004279f1:
audio_at_004279f2:
    xor eax,eax ;source 004279F2
audio_at_004279f4:
audio_at_004279f5:
audio_at_004279f7:
audio_at_004279f8:
    jmp audio_finished ;source 004279F8
audio_at_004279f9:
    fld DWORD PTR [audio_00477670] ;source 004279F9
audio_at_004279ff:
    fmul DWORD PTR [rsp+172] ;source 004279FF
audio_at_00427a02:
    fld DWORD PTR [audio_0047766c] ;source 00427A02
audio_at_00427a08:
    fmul DWORD PTR [rsp+168] ;source 00427A08
audio_at_00427a0b:
    faddp st(1),st(0) ;source 00427A0B
audio_at_00427a0d:
    fld DWORD PTR [audio_00477668] ;source 00427A0D
audio_at_00427a13:
    fmul DWORD PTR [rsp+164] ;source 00427A13
audio_at_00427a16:
    faddp st(1),st(0) ;source 00427A16
audio_at_00427a18:
    fstp DWORD PTR [rsp+180] ;source 00427A18
audio_at_00427a1b:
    mov eax,DWORD PTR [rsp+180] ;source 00427A1B
audio_at_00427a1e:
    test eax,eax ;source 00427A1E
audio_at_00427a20:
    jl audio_at_00427a2a ;source 00427A20
audio_at_00427a22:
audio_at_00427a23:
    xor eax,eax ;source 00427A23
audio_at_00427a25:
audio_at_00427a26:
audio_at_00427a28:
audio_at_00427a29:
    jmp audio_finished ;source 00427A29
audio_at_00427a2a:
    fld DWORD PTR [audio_00477680] ;source 00427A2A
audio_at_00427a30:
    fmul DWORD PTR [rsp+172] ;source 00427A30
audio_at_00427a33:
    fld DWORD PTR [audio_0047767c] ;source 00427A33
audio_at_00427a39:
    fmul DWORD PTR [rsp+168] ;source 00427A39
audio_at_00427a3c:
    faddp st(1),st(0) ;source 00427A3C
audio_at_00427a3e:
    fld DWORD PTR [audio_00477678] ;source 00427A3E
audio_at_00427a44:
    fmul DWORD PTR [rsp+164] ;source 00427A44
audio_at_00427a47:
    faddp st(1),st(0) ;source 00427A47
audio_at_00427a49:
    fstp DWORD PTR [rsp+180] ;source 00427A49
audio_at_00427a4c:
    mov eax,DWORD PTR [rsp+180] ;source 00427A4C
audio_at_00427a4f:
    test eax,eax ;source 00427A4F
audio_at_00427a51:
    jl audio_at_00427a5b ;source 00427A51
audio_at_00427a53:
audio_at_00427a54:
    xor eax,eax ;source 00427A54
audio_at_00427a56:
audio_at_00427a57:
audio_at_00427a59:
audio_at_00427a5a:
    jmp audio_finished ;source 00427A5A
audio_at_00427a5b:
    fld DWORD PTR [audio_00477738] ;source 00427A5B
audio_at_00427a61:
    fmul DWORD PTR [rsi + 16] ;source 00427A61
audio_at_00427a64:
    fld DWORD PTR [audio_0047773c] ;source 00427A64
audio_at_00427a6a:
    fmul DWORD PTR [rsi + 20] ;source 00427A6A
audio_at_00427a6d:
    fstp DWORD PTR [rsp+168] ;source 00427A6D
audio_at_00427a70:
    fld DWORD PTR [audio_00477740] ;source 00427A70
audio_at_00427a76:
    fmul DWORD PTR [rsi + 24] ;source 00427A76
audio_at_00427a79:
    fstp DWORD PTR [rsp+172] ;source 00427A79
audio_at_00427a7c:
    fadd DWORD PTR [rsi] ;source 00427A7C
audio_at_00427a7e:
    fld DWORD PTR [rsp+168] ;source 00427A7E
audio_at_00427a81:
    fadd DWORD PTR [rsi + 4] ;source 00427A81
audio_at_00427a84:
    fstp DWORD PTR [rsp+168] ;source 00427A84
audio_at_00427a87:
    fld DWORD PTR [rsp+172] ;source 00427A87
audio_at_00427a8a:
    fadd DWORD PTR [rsi + 8] ;source 00427A8A
audio_at_00427a8d:
    fstp DWORD PTR [rsp+172] ;source 00427A8D
audio_at_00427a90:
    fld DWORD PTR [rsp+148] ;source 00427A90
audio_at_00427a93:
    fsub st(0),st(1) ;source 00427A93
audio_at_00427a95:
    fstp DWORD PTR [rsp+164] ;source 00427A95
audio_at_00427a98:
    fstp st(0) ;source 00427A98
audio_at_00427a9a:
    fld DWORD PTR [rsp+152] ;source 00427A9A
audio_at_00427a9d:
    fsub DWORD PTR [rsp+168] ;source 00427A9D
audio_at_00427aa0:
    fstp DWORD PTR [rsp+168] ;source 00427AA0
audio_at_00427aa3:
    fld DWORD PTR [rsp+156] ;source 00427AA3
audio_at_00427aa6:
    fsub DWORD PTR [rsp+172] ;source 00427AA6
audio_at_00427aa9:
    fstp DWORD PTR [rsp+172] ;source 00427AA9
audio_at_00427aac:
    fld DWORD PTR [audio_00477690] ;source 00427AAC
audio_at_00427ab2:
    fmul DWORD PTR [rsp+172] ;source 00427AB2
audio_at_00427ab5:
    fld DWORD PTR [audio_0047768c] ;source 00427AB5
audio_at_00427abb:
    fmul DWORD PTR [rsp+168] ;source 00427ABB
audio_at_00427abe:
    faddp st(1),st(0) ;source 00427ABE
audio_at_00427ac0:
    fld DWORD PTR [audio_00477688] ;source 00427AC0
audio_at_00427ac6:
    fmul DWORD PTR [rsp+164] ;source 00427AC6
audio_at_00427ac9:
    faddp st(1),st(0) ;source 00427AC9
audio_at_00427acb:
    fstp DWORD PTR [rsp+180] ;source 00427ACB
audio_at_00427ace:
    mov eax,DWORD PTR [rsp+180] ;source 00427ACE
audio_at_00427ad1:
    test eax,eax ;source 00427AD1
audio_at_00427ad3:
    jl audio_at_00427add ;source 00427AD3
audio_at_00427ad5:
audio_at_00427ad6:
    xor eax,eax ;source 00427AD6
audio_at_00427ad8:
audio_at_00427ad9:
audio_at_00427adb:
audio_at_00427adc:
    jmp audio_finished ;source 00427ADC
audio_at_00427add:
    fld DWORD PTR [audio_004776a0] ;source 00427ADD
audio_at_00427ae3:
    fmul DWORD PTR [rsp+172] ;source 00427AE3
audio_at_00427ae6:
    fld DWORD PTR [audio_0047769c] ;source 00427AE6
audio_at_00427aec:
    fmul DWORD PTR [rsp+168] ;source 00427AEC
audio_at_00427aef:
    faddp st(1),st(0) ;source 00427AEF
audio_at_00427af1:
    fld DWORD PTR [audio_00477698] ;source 00427AF1
audio_at_00427af7:
    fmul DWORD PTR [rsp+164] ;source 00427AF7
audio_at_00427afa:
    faddp st(1),st(0) ;source 00427AFA
audio_at_00427afc:
    fstp DWORD PTR [rsp+180] ;source 00427AFC
audio_at_00427aff:
    mov eax,DWORD PTR [rsp+180] ;source 00427AFF
audio_at_00427b02:
    test eax,eax ;source 00427B02
audio_at_00427b04:
    jl audio_at_00427b0e ;source 00427B04
audio_at_00427b06:
audio_at_00427b07:
    xor eax,eax ;source 00427B07
audio_at_00427b09:
audio_at_00427b0a:
audio_at_00427b0c:
audio_at_00427b0d:
    jmp audio_finished ;source 00427B0D
audio_at_00427b0e:
    fld DWORD PTR [audio_004776b0] ;source 00427B0E
audio_at_00427b14:
    fmul DWORD PTR [rsp+172] ;source 00427B14
audio_at_00427b17:
    fld DWORD PTR [audio_004776ac] ;source 00427B17
audio_at_00427b1d:
    fmul DWORD PTR [rsp+168] ;source 00427B1D
audio_at_00427b20:
    xor eax,eax ;source 00427B20
audio_at_00427b22:
audio_at_00427b23:
    faddp st(1),st(0) ;source 00427B23
audio_at_00427b25:
    fld DWORD PTR [audio_004776a8] ;source 00427B25
audio_at_00427b2b:
    fmul DWORD PTR [rsp+164] ;source 00427B2B
audio_at_00427b2e:
audio_at_00427b2f:
    faddp st(1),st(0) ;source 00427B2F
audio_at_00427b31:
    fstp DWORD PTR [rsp+180] ;source 00427B31
audio_at_00427b34:
    mov ecx,DWORD PTR [rsp+180] ;source 00427B34
audio_at_00427b37:
    test ecx,ecx ;source 00427B37
audio_at_00427b39:
    setl al ;source 00427B39
audio_at_00427b3c:
audio_at_00427b3e:
audio_at_00427b3f:
    jmp audio_finished ;source 00427B3F
audio_finished:
    add rsp,0b8h
    pop rdi
    pop rsi
    ret
sgn_source_audio_contains_4278a0 ENDP
END
