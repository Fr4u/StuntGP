option casemap:none
; Native64 host ABI leaves: no native callbacks inside these numeric regions.
; Original source instructions are recorded at every emitted instruction group.
; x87 CW is inherited. Only462860 temporarily selects truncation, then restores it.
.const
score_screen_y DWORD 043700000h ;PE46E1D0
score_screen_x DWORD 043a00000h ;PE46E1D4
score_half DWORD 03f000000h ;PE46E1D8
score_noise_y DWORD 040f00000h ;PE46E1DC
score_timer DWORD 042700000h ;PE46E1E0
score_scale DWORD 0437f0000h ;PE46E1E4
score_turn DWORD 043b40000h ;PE46E1E8
score_zero DWORD 000000000h ;PE46E1EC
score_terminal_y DWORD 04326aaabh ;PE46E1F0
score_speed_squared DWORD 046d9038eh ;PE46E204
score_random_scale DWORD 038000100h ;PE46E714
.code

;Full462860, consuming ST0. Low DWORD is returned even for x87 indefinite.
score_ftol MACRO
    fstcw WORD PTR [rsp+128]
    fwait
    mov ax,WORD PTR [rsp+128]
    or ax,0c00h
    mov WORD PTR [rsp+130],ax
    fldcw WORD PTR [rsp+130]
    fistp QWORD PTR [rsp+136]
    fldcw WORD PTR [rsp+128]
    mov eax,DWORD PTR [rsp+136]
    mov edx,DWORD PTR [rsp+140]
ENDM

;Full422620. ECX/EDX are the original two colors, EAX is the PUSHed amount.
;All IMUL/SHL/ADD/OR stay DWORD, including the signed blue SAR.
score_blend MACRO
    mov r8d,eax
    mov r9d,ecx
    shr r9d,16
    and r9d,0ffh
    mov r11d,edx
    shr r11d,16
    and r11d,0ffh
    sub r11d,r9d
    imul r11d,r8d
    and r11d,0ffffff00h
    shl r9d,8
    add r11d,r9d
    shl r11d,8
    mov eax,r11d
    mov r9d,ecx
    shr r9d,8
    and r9d,0ffh
    mov r11d,edx
    shr r11d,8
    and r11d,0ffh
    sub r11d,r9d
    imul r11d,r8d
    and r11d,0ffffff00h
    shl r9d,8
    add r11d,r9d
    or eax,r11d
    and ecx,0ffh
    and edx,0ffh
    sub edx,ecx
    imul edx,r8d
    sar edx,8
    add edx,ecx
    or eax,edx
ENDM

;Full401D70: RCX actual popup.offset, R11 actual482678 gravity, EAX scale bits.
;Locals are private spill slots, never a copied vector owner.
score_gravity MACRO
    LOCAL gravity_done
    mov DWORD PTR [rsp+120],eax
    fld DWORD PTR [rcx+8]
    fld DWORD PTR [rcx+4]
    fld DWORD PTR [rcx]
    fld st(0)
    fmul st(0),st(1)
    fld st(2)
    fmul st(0),st(3)
    faddp st(1),st(0)
    fld st(3)
    fmul st(0),st(4)
    faddp st(1),st(0)
    fcomp DWORD PTR [score_speed_squared]
    fstp st(0)
    fnstsw ax
    fstp st(0)
    test ah,41h
    fstp st(0)
    jz gravity_done
    fld DWORD PTR [r11]
    fmul DWORD PTR [rsp+120]
    fld DWORD PTR [r11+4]
    fmul DWORD PTR [rsp+120]
    fstp DWORD PTR [rsp+124]
    fld DWORD PTR [r11+8]
    fmul DWORD PTR [rsp+120]
    fstp DWORD PTR [rsp+144]
    fadd DWORD PTR [rcx]
    fstp DWORD PTR [rcx]
    fld DWORD PTR [rsp+124]
    fadd DWORD PTR [rcx+4]
    fstp DWORD PTR [rcx+4]
    fld DWORD PTR [rsp+144]
    fadd DWORD PTR [rcx+8]
    fstp DWORD PTR [rcx+8]
gravity_done:
ENDM

;Full463289 then42F1C0. R11 is the SAME VehicleRandom.state4959C0.
;[rsp+4] is the original float argument. Return remains on x87 ST0.
score_random MACRO
    mov eax,DWORD PTR [r11]
    imul eax,eax,343fdh
    add eax,269ec3h
    mov DWORD PTR [r11],eax
    sar eax,16
    and eax,7fffh
    sub eax,3fffh
    mov DWORD PTR [rsp],eax
    fild DWORD PTR [rsp]
    fmul DWORD PTR [rsp+4]
    fmul DWORD PTR [score_random_scale]
ENDM

PUBLIC sgn_source_score_prefix_40179a
sgn_source_score_prefix_40179a PROC FRAME
    sub rsp,152
    .allocstack 152
    .endprolog
    lea r10,[rcx+18h]
    mov r11,rdx
prefix_0040179a: ;0040179A FLD float ptr [ESI + 0x98]
    FLD DWORD PTR [R10 + 098h]
prefix_004017a0: ;004017A0 FADD float ptr [ESI + -0x4]
    FADD DWORD PTR [R10 + -04h]
prefix_004017a3: ;004017A3 LEA ECX,[ESI + -0x4]
    lea rcx,[r10-4]
prefix_004017a6: ;004017A6 FSTP float ptr [ESI + 0x98]
    FSTP DWORD PTR [R10 + 098h]
prefix_004017ac: ;004017AC FLD float ptr [ESI + 0x9c]
    FLD DWORD PTR [R10 + 09ch]
prefix_004017b2: ;004017B2 FADD float ptr [ESI]
    FADD DWORD PTR [R10]
prefix_004017b4: ;004017B4 FSTP float ptr [ESI + 0x9c]
    FSTP DWORD PTR [R10 + 09ch]
prefix_004017ba: ;004017BA FLD float ptr [ESI]
    FLD DWORD PTR [R10]
prefix_004017bc: ;004017BC FCOMP float ptr [0x0046e1f0]
    FCOMP DWORD PTR [score_terminal_y]
prefix_004017c2: ;004017C2 FNSTSW AX
    FNSTSW AX
prefix_004017c4: ;004017C4 TEST AH,0x1
    TEST AH,01h
prefix_004017c7: ;004017C7 JZ 0x004017d2
    JZ prefix_004017d2
prefix_004017c9: ;004017C9 MOV EAX,dword ptr [ESI + 0xc]
    MOV EAX,DWORD PTR [R10 + 0ch]
prefix_004017cc: ;004017CC PUSH EAX
    ;Argument stays in EAX for the immediate leaf macro.
prefix_004017cd: ;004017CD CALL 0x00401d70
    score_gravity
prefix_004017d2: ;004017D2 TEST byte ptr [ESI + -0x8],0x10
    TEST BYTE PTR [R10 + -08h],010h
prefix_004017d6: ;004017D6 JZ 0x00401831
    JZ prefix_00401831
prefix_004017d8: ;004017D8 MOV EDX,dword ptr [ESI + 0x10]
    MOV EDX,DWORD PTR [R10 + 010h]
prefix_004017db: ;004017DB MOV dword ptr [EBP + -0xc],EDI
    MOV DWORD PTR [rsp+108],0
prefix_004017de: ;004017DE MOV dword ptr [EBP + -0x10],EDX
    MOV DWORD PTR [rsp+104],EDX
prefix_004017e1: ;004017E1 FILD qword ptr [EBP + -0x10]
    FILD QWORD PTR [rsp+104]
prefix_004017e4: ;004017E4 FCOM float ptr [ESI + 0x40]
    FCOM DWORD PTR [R10 + 040h]
prefix_004017e7: ;004017E7 FNSTSW AX
    FNSTSW AX
prefix_004017e9: ;004017E9 TEST AH,0x1
    TEST AH,01h
prefix_004017ec: ;004017EC JZ 0x004017f9
    JZ prefix_004017f9
prefix_004017ee: ;004017EE FMUL float ptr [ESI + 0x3c]
    FMUL DWORD PTR [R10 + 03ch]
prefix_004017f1: ;004017F1 FDIV float ptr [ESI + 0x40]
    FDIV DWORD PTR [R10 + 040h]
prefix_004017f4: ;004017F4 FSTP float ptr [ESI + 0x48]
    FSTP DWORD PTR [R10 + 048h]
prefix_004017f7: ;004017F7 JMP 0x00401837
    JMP prefix_00401837
prefix_004017f9: ;004017F9 MOV ECX,dword ptr [ESI + 0x14]
    MOV ECX,DWORD PTR [R10 + 014h]
prefix_004017fc: ;004017FC MOV dword ptr [EBP + -0x14],EDI
    MOV DWORD PTR [rsp+100],0
prefix_004017ff: ;004017FF MOV dword ptr [EBP + -0x18],ECX
    MOV DWORD PTR [rsp+96],ECX
prefix_00401802: ;00401802 FILD qword ptr [EBP + -0x18]
    FILD QWORD PTR [rsp+96]
prefix_00401805: ;00401805 FSUB float ptr [ESI + 0x44]
    FSUB DWORD PTR [R10 + 044h]
prefix_00401808: ;00401808 FXCH
    FXCH st(1)
prefix_0040180a: ;0040180A FCOMPP
    FCOMPP
prefix_0040180c: ;0040180C FNSTSW AX
    FNSTSW AX
prefix_0040180e: ;0040180E TEST AH,0x41
    TEST AH,041h
prefix_00401811: ;00401811 JNZ 0x00401829
    JNZ prefix_00401829
prefix_00401813: ;00401813 SUB ECX,EDX
    SUB ECX,EDX
prefix_00401815: ;00401815 MOV dword ptr [EBP + -0x1c],EDI
    MOV DWORD PTR [rsp+92],0
prefix_00401818: ;00401818 MOV dword ptr [EBP + -0x20],ECX
    MOV DWORD PTR [rsp+88],ECX
prefix_0040181b: ;0040181B FILD qword ptr [EBP + -0x20]
    FILD QWORD PTR [rsp+88]
prefix_0040181e: ;0040181E FMUL float ptr [ESI + 0x3c]
    FMUL DWORD PTR [R10 + 03ch]
prefix_00401821: ;00401821 FDIV float ptr [ESI + 0x44]
    FDIV DWORD PTR [R10 + 044h]
prefix_00401824: ;00401824 FSTP float ptr [ESI + 0x48]
    FSTP DWORD PTR [R10 + 048h]
prefix_00401827: ;00401827 JMP 0x00401837
    JMP prefix_00401837
prefix_00401829: ;00401829 FLD float ptr [ESI + 0x3c]
    FLD DWORD PTR [R10 + 03ch]
prefix_0040182c: ;0040182C FSTP float ptr [ESI + 0x48]
    FSTP DWORD PTR [R10 + 048h]
prefix_0040182f: ;0040182F JMP 0x00401837
    JMP prefix_00401837
prefix_00401831: ;00401831 MOV ECX,dword ptr [ESI + 0x3c]
    MOV ECX,DWORD PTR [R10 + 03ch]
prefix_00401834: ;00401834 MOV dword ptr [ESI + 0x48],ECX
    MOV DWORD PTR [R10 + 048h],ECX
prefix_00401837: ;00401837 TEST byte ptr [ESI + -0x8],0x8
    TEST BYTE PTR [R10 + -08h],08h
prefix_0040183b: ;0040183B JZ 0x00401896
    JZ prefix_00401896
prefix_0040183d: ;0040183D MOV EDX,dword ptr [ESI + 0x10]
    MOV EDX,DWORD PTR [R10 + 010h]
prefix_00401840: ;00401840 MOV dword ptr [EBP + -0x24],EDI
    MOV DWORD PTR [rsp+84],0
prefix_00401843: ;00401843 MOV dword ptr [EBP + -0x28],EDX
    MOV DWORD PTR [rsp+80],EDX
prefix_00401846: ;00401846 FILD qword ptr [EBP + -0x28]
    FILD QWORD PTR [rsp+80]
prefix_00401849: ;00401849 FCOM float ptr [ESI + 0x30]
    FCOM DWORD PTR [R10 + 030h]
prefix_0040184c: ;0040184C FNSTSW AX
    FNSTSW AX
prefix_0040184e: ;0040184E TEST AH,0x1
    TEST AH,01h
prefix_00401851: ;00401851 JZ 0x0040185e
    JZ prefix_0040185e
prefix_00401853: ;00401853 FMUL float ptr [ESI + 0x2c]
    FMUL DWORD PTR [R10 + 02ch]
prefix_00401856: ;00401856 FDIV float ptr [ESI + 0x30]
    FDIV DWORD PTR [R10 + 030h]
prefix_00401859: ;00401859 FSTP float ptr [ESI + 0x38]
    FSTP DWORD PTR [R10 + 038h]
prefix_0040185c: ;0040185C JMP 0x0040189c
    JMP prefix_0040189c
prefix_0040185e: ;0040185E MOV ECX,dword ptr [ESI + 0x14]
    MOV ECX,DWORD PTR [R10 + 014h]
prefix_00401861: ;00401861 MOV dword ptr [EBP + -0x2c],EDI
    MOV DWORD PTR [rsp+76],0
prefix_00401864: ;00401864 MOV dword ptr [EBP + -0x30],ECX
    MOV DWORD PTR [rsp+72],ECX
prefix_00401867: ;00401867 FILD qword ptr [EBP + -0x30]
    FILD QWORD PTR [rsp+72]
prefix_0040186a: ;0040186A FSUB float ptr [ESI + 0x34]
    FSUB DWORD PTR [R10 + 034h]
prefix_0040186d: ;0040186D FXCH
    FXCH st(1)
prefix_0040186f: ;0040186F FCOMPP
    FCOMPP
prefix_00401871: ;00401871 FNSTSW AX
    FNSTSW AX
prefix_00401873: ;00401873 TEST AH,0x41
    TEST AH,041h
prefix_00401876: ;00401876 JNZ 0x0040188e
    JNZ prefix_0040188e
prefix_00401878: ;00401878 SUB ECX,EDX
    SUB ECX,EDX
prefix_0040187a: ;0040187A MOV dword ptr [EBP + -0x34],EDI
    MOV DWORD PTR [rsp+68],0
prefix_0040187d: ;0040187D MOV dword ptr [EBP + -0x38],ECX
    MOV DWORD PTR [rsp+64],ECX
prefix_00401880: ;00401880 FILD qword ptr [EBP + -0x38]
    FILD QWORD PTR [rsp+64]
prefix_00401883: ;00401883 FMUL float ptr [ESI + 0x2c]
    FMUL DWORD PTR [R10 + 02ch]
prefix_00401886: ;00401886 FDIV float ptr [ESI + 0x34]
    FDIV DWORD PTR [R10 + 034h]
prefix_00401889: ;00401889 FSTP float ptr [ESI + 0x38]
    FSTP DWORD PTR [R10 + 038h]
prefix_0040188c: ;0040188C JMP 0x0040189c
    JMP prefix_0040189c
prefix_0040188e: ;0040188E FLD float ptr [ESI + 0x2c]
    FLD DWORD PTR [R10 + 02ch]
prefix_00401891: ;00401891 FSTP float ptr [ESI + 0x38]
    FSTP DWORD PTR [R10 + 038h]
prefix_00401894: ;00401894 JMP 0x0040189c
    JMP prefix_0040189c
prefix_00401896: ;00401896 MOV EDX,dword ptr [ESI + 0x2c]
    MOV EDX,DWORD PTR [R10 + 02ch]
prefix_00401899: ;00401899 MOV dword ptr [ESI + 0x38],EDX
    MOV DWORD PTR [R10 + 038h],EDX
prefix_0040189c: ;0040189C TEST byte ptr [ESI + -0x8],BL
    TEST BYTE PTR [R10 + -08h],1
prefix_0040189f: ;0040189F JZ 0x0040193a
    JZ prefix_0040193a
prefix_004018a5: ;004018A5 MOV EAX,dword ptr [ESI + 0x10]
    MOV EAX,DWORD PTR [R10 + 010h]
prefix_004018a8: ;004018A8 MOV dword ptr [EBP + -0x3c],EDI
    MOV DWORD PTR [rsp+60],0
prefix_004018ab: ;004018AB MOV dword ptr [EBP + -0x40],EAX
    MOV DWORD PTR [rsp+56],EAX
prefix_004018ae: ;004018AE FILD qword ptr [EBP + -0x40]
    FILD QWORD PTR [rsp+56]
prefix_004018b1: ;004018B1 FCOM float ptr [ESI + 0x50]
    FCOM DWORD PTR [R10 + 050h]
prefix_004018b4: ;004018B4 FNSTSW AX
    FNSTSW AX
prefix_004018b6: ;004018B6 TEST AH,0x1
    TEST AH,01h
prefix_004018b9: ;004018B9 JZ 0x004018c6
    JZ prefix_004018c6
prefix_004018bb: ;004018BB FSUBR float ptr [ESI + 0x50]
    FSUBR DWORD PTR [R10 + 050h]
prefix_004018be: ;004018BE FMUL float ptr [ESI + 0x4c]
    FMUL DWORD PTR [R10 + 04ch]
prefix_004018c1: ;004018C1 FDIV float ptr [ESI + 0x50]
    FDIV DWORD PTR [R10 + 050h]
prefix_004018c4: ;004018C4 JMP 0x004018fd
    JMP prefix_004018fd
prefix_004018c6: ;004018C6 MOV ECX,dword ptr [ESI + 0x14]
    MOV ECX,DWORD PTR [R10 + 014h]
prefix_004018c9: ;004018C9 MOV dword ptr [EBP + -0x44],EDI
    MOV DWORD PTR [rsp+52],0
prefix_004018cc: ;004018CC MOV dword ptr [EBP + -0x48],ECX
    MOV DWORD PTR [rsp+48],ECX
prefix_004018cf: ;004018CF FILD qword ptr [EBP + -0x48]
    FILD QWORD PTR [rsp+48]
prefix_004018d2: ;004018D2 FSUB float ptr [ESI + 0x54]
    FSUB DWORD PTR [R10 + 054h]
prefix_004018d5: ;004018D5 FLD ST1
    FLD st(1)
prefix_004018d7: ;004018D7 FCOMP
    FCOMP st(1)
prefix_004018d9: ;004018D9 FNSTSW AX
    FNSTSW AX
prefix_004018db: ;004018DB TEST AH,0x41
    TEST AH,041h
prefix_004018de: ;004018DE JNZ 0x004018f0
    JNZ prefix_004018f0
prefix_004018e0: ;004018E0 FXCH
    FXCH st(1)
prefix_004018e2: ;004018E2 FSUB ST0,ST1
    FSUB st(0),st(1)
prefix_004018e4: ;004018E4 FMUL float ptr [ESI + 0x4c]
    FMUL DWORD PTR [R10 + 04ch]
prefix_004018e7: ;004018E7 FDIV float ptr [ESI + 0x54]
    FDIV DWORD PTR [R10 + 054h]
prefix_004018ea: ;004018EA FXCH
    FXCH st(1)
prefix_004018ec: ;004018EC FSTP ST0
    FSTP st(0)
prefix_004018ee: ;004018EE JMP 0x004018fd
    JMP prefix_004018fd
prefix_004018f0: ;004018F0 FSTP ST0
    FSTP st(0)
prefix_004018f2: ;004018F2 FSTP ST0
    FSTP st(0)
prefix_004018f4: ;004018F4 FLD float ptr [0x0046e1ec]
    FLD DWORD PTR [score_zero]
prefix_004018fa: ;004018FA MOV dword ptr [ESI + 0x58],EDI
    MOV DWORD PTR [R10 + 058h],0
prefix_004018fd: ;004018FD FADD float ptr [ESI + 0x58]
    FADD DWORD PTR [R10 + 058h]
prefix_00401900: ;00401900 FST float ptr [EBP + -0x4]
    FST DWORD PTR [rsp+116]
prefix_00401903: ;00401903 FSTP float ptr [ESI + 0x58]
    FSTP DWORD PTR [R10 + 058h]
prefix_00401906: ;00401906 FLD float ptr [EBP + -0x4]
    FLD DWORD PTR [rsp+116]
prefix_00401909: ;00401909 FCOMP float ptr [0x0046e1e8]
    FCOMP DWORD PTR [score_turn]
prefix_0040190f: ;0040190F FLD float ptr [EBP + -0x4]
    FLD DWORD PTR [rsp+116]
prefix_00401912: ;00401912 FNSTSW AX
    FNSTSW AX
prefix_00401914: ;00401914 TEST AH,0x1
    TEST AH,01h
prefix_00401917: ;00401917 JNZ 0x00401921
    JNZ prefix_00401921
prefix_00401919: ;00401919 FSUB float ptr [0x0046e1e8]
    FSUB DWORD PTR [score_turn]
prefix_0040191f: ;0040191F JMP 0x00401937
    JMP prefix_00401937
prefix_00401921: ;00401921 FCOMP float ptr [0x0046e1ec]
    FCOMP DWORD PTR [score_zero]
prefix_00401927: ;00401927 FNSTSW AX
    FNSTSW AX
prefix_00401929: ;00401929 TEST AH,0x1
    TEST AH,01h
prefix_0040192c: ;0040192C JZ 0x0040193a
    JZ prefix_0040193a
prefix_0040192e: ;0040192E FLD float ptr [EBP + -0x4]
    FLD DWORD PTR [rsp+116]
prefix_00401931: ;00401931 FADD float ptr [0x0046e1e8]
    FADD DWORD PTR [score_turn]
prefix_00401937: ;00401937 FSTP float ptr [ESI + 0x58]
    FSTP DWORD PTR [R10 + 058h]
prefix_0040193a: ;0040193A TEST byte ptr [ESI + -0x8],0x20
    TEST BYTE PTR [R10 + -08h],020h
prefix_0040193e: ;0040193E JZ 0x004019e8
    JZ prefix_004019e8
prefix_00401944: ;00401944 MOV ECX,dword ptr [ESI + 0x10]
    MOV ECX,DWORD PTR [R10 + 010h]
prefix_00401947: ;00401947 MOV dword ptr [EBP + -0x4c],EDI
    MOV DWORD PTR [rsp+44],0
prefix_0040194a: ;0040194A MOV dword ptr [EBP + -0x50],ECX
    MOV DWORD PTR [rsp+40],ECX
prefix_0040194d: ;0040194D FILD qword ptr [EBP + -0x50]
    FILD QWORD PTR [rsp+40]
prefix_00401950: ;00401950 FCOM float ptr [ESI + 0x5c]
    FCOM DWORD PTR [R10 + 05ch]
prefix_00401953: ;00401953 FNSTSW AX
    FNSTSW AX
prefix_00401955: ;00401955 TEST AH,0x1
    TEST AH,01h
prefix_00401958: ;00401958 JZ 0x00401988
    JZ prefix_00401988
prefix_0040195a: ;0040195A MOV EDX,ECX
    MOV EDX,ECX
prefix_0040195c: ;0040195C MOV dword ptr [EBP + -0x54],EDI
    MOV DWORD PTR [rsp+36],0
prefix_0040195f: ;0040195F SHL EDX,0x8
    SHL EDX,08h
prefix_00401962: ;00401962 SUB EDX,ECX
    SUB EDX,ECX
prefix_00401964: ;00401964 FSTP ST0
    FSTP st(0)
prefix_00401966: ;00401966 MOV dword ptr [EBP + -0x58],EDX
    MOV DWORD PTR [rsp+32],EDX
prefix_00401969: ;00401969 FILD qword ptr [EBP + -0x58]
    FILD QWORD PTR [rsp+32]
prefix_0040196c: ;0040196C FDIV float ptr [ESI + 0x5c]
    FDIV DWORD PTR [R10 + 05ch]
prefix_0040196f: ;0040196F CALL 0x00462860
    score_ftol
prefix_00401974: ;00401974 MOV EDX,dword ptr [ESI + 0x68]
    MOV EDX,DWORD PTR [R10 + 068h]
prefix_00401977: ;00401977 MOV ECX,dword ptr [ESI + 0x64]
    MOV ECX,DWORD PTR [R10 + 064h]
prefix_0040197a: ;0040197A PUSH EAX
    ;Argument stays in EAX for the immediate leaf macro.
prefix_0040197b: ;0040197B CALL 0x00422620
    score_blend
prefix_00401980: ;00401980 MOV dword ptr [ESI + 0xdc],EAX
    MOV DWORD PTR [R10 + 0dch],EAX
prefix_00401986: ;00401986 JMP 0x004019f1
    JMP prefix_004019f1
prefix_00401988: ;00401988 MOV EDX,dword ptr [ESI + 0x14]
    MOV EDX,DWORD PTR [R10 + 014h]
prefix_0040198b: ;0040198B MOV dword ptr [EBP + -0x5c],EDI
    MOV DWORD PTR [rsp+28],0
prefix_0040198e: ;0040198E MOV dword ptr [EBP + -0x60],EDX
    MOV DWORD PTR [rsp+24],EDX
prefix_00401991: ;00401991 FILD qword ptr [EBP + -0x60]
    FILD QWORD PTR [rsp+24]
prefix_00401994: ;00401994 FSUB float ptr [ESI + 0x60]
    FSUB DWORD PTR [R10 + 060h]
prefix_00401997: ;00401997 FXCH
    FXCH st(1)
prefix_00401999: ;00401999 FCOMPP
    FCOMPP
prefix_0040199b: ;0040199B FNSTSW AX
    FNSTSW AX
prefix_0040199d: ;0040199D TEST AH,0x41
    TEST AH,041h
prefix_004019a0: ;004019A0 JNZ 0x004019dd
    JNZ prefix_004019dd
prefix_004019a2: ;004019A2 MOV EAX,EDX
    MOV EAX,EDX
prefix_004019a4: ;004019A4 MOV dword ptr [EBP + -0x64],EDI
    MOV DWORD PTR [rsp+20],0
prefix_004019a7: ;004019A7 SHL EAX,0x8
    SHL EAX,08h
prefix_004019aa: ;004019AA SUB EAX,EDX
    SUB EAX,EDX
prefix_004019ac: ;004019AC MOV EDX,ECX
    MOV EDX,ECX
prefix_004019ae: ;004019AE SHL EDX,0x8
    SHL EDX,08h
prefix_004019b1: ;004019B1 SUB EDX,ECX
    SUB EDX,ECX
prefix_004019b3: ;004019B3 SUB EAX,EDX
    SUB EAX,EDX
prefix_004019b5: ;004019B5 MOV dword ptr [EBP + -0x68],EAX
    MOV DWORD PTR [rsp+16],EAX
prefix_004019b8: ;004019B8 FILD qword ptr [EBP + -0x68]
    FILD QWORD PTR [rsp+16]
prefix_004019bb: ;004019BB FDIV float ptr [ESI + 0x60]
    FDIV DWORD PTR [R10 + 060h]
prefix_004019be: ;004019BE FSUBR float ptr [0x0046e1e4]
    FSUBR DWORD PTR [score_scale]
prefix_004019c4: ;004019C4 CALL 0x00462860
    score_ftol
prefix_004019c9: ;004019C9 MOV EDX,dword ptr [ESI + 0x6c]
    MOV EDX,DWORD PTR [R10 + 06ch]
prefix_004019cc: ;004019CC MOV ECX,dword ptr [ESI + 0x68]
    MOV ECX,DWORD PTR [R10 + 068h]
prefix_004019cf: ;004019CF PUSH EAX
    ;Argument stays in EAX for the immediate leaf macro.
prefix_004019d0: ;004019D0 CALL 0x00422620
    score_blend
prefix_004019d5: ;004019D5 MOV dword ptr [ESI + 0xdc],EAX
    MOV DWORD PTR [R10 + 0dch],EAX
prefix_004019db: ;004019DB JMP 0x004019f1
    JMP prefix_004019f1
prefix_004019dd: ;004019DD MOV EAX,dword ptr [ESI + 0x68]
    MOV EAX,DWORD PTR [R10 + 068h]
prefix_004019e0: ;004019E0 MOV dword ptr [ESI + 0xdc],EAX
    MOV DWORD PTR [R10 + 0dch],EAX
prefix_004019e6: ;004019E6 JMP 0x004019f1
    JMP prefix_004019f1
prefix_004019e8: ;004019E8 MOV ECX,dword ptr [ESI + 0x64]
    MOV ECX,DWORD PTR [R10 + 064h]
prefix_004019eb: ;004019EB MOV dword ptr [ESI + 0xdc],ECX
    MOV DWORD PTR [R10 + 0dch],ECX
prefix_004019f1:
    add rsp,152
    ret
sgn_source_score_prefix_40179a ENDP

PUBLIC sgn_source_score_overlay_401a10
sgn_source_score_overlay_401a10 PROC FRAME
    sub rsp,152
    .allocstack 152
    .endprolog
    lea r10,[rcx+18h]
    mov r11,rdx
overlay_00401a10: ;00401A10 CMP dword ptr [ESI + 0x80],EBX
    CMP DWORD PTR [R10 + 080h],1
overlay_00401a16: ;00401A16 JNZ 0x00401a93
    JNZ overlay_00401a93
overlay_00401a18: ;00401A18 MOV EDX,dword ptr [ESI + 0x10]
    MOV EDX,DWORD PTR [R10 + 010h]
overlay_00401a1b: ;00401A1B MOV dword ptr [EBP + -0x6c],EDI
    MOV DWORD PTR [rsp+12],0
overlay_00401a1e: ;00401A1E MOV dword ptr [EBP + -0x70],EDX
    MOV DWORD PTR [rsp+8],EDX
overlay_00401a21: ;00401A21 FILD qword ptr [EBP + -0x70]
    FILD QWORD PTR [rsp+8]
overlay_00401a24: ;00401A24 FCOM float ptr [ESI + 0x8c]
    FCOM DWORD PTR [R10 + 08ch]
overlay_00401a2a: ;00401A2A FNSTSW AX
    FNSTSW AX
overlay_00401a2c: ;00401A2C TEST AH,0x1
    TEST AH,01h
overlay_00401a2f: ;00401A2F JNZ 0x00401a91
    JNZ overlay_00401a91
overlay_00401a31: ;00401A31 MOV EAX,dword ptr [ESI + 0x14]
    MOV EAX,DWORD PTR [R10 + 014h]
overlay_00401a34: ;00401A34 MOV dword ptr [EBP + -0x74],EDI
    MOV DWORD PTR [rsp+4],0
overlay_00401a37: ;00401A37 MOV dword ptr [EBP + -0x78],EAX
    MOV DWORD PTR [rsp+0],EAX
overlay_00401a3a: ;00401A3A FILD qword ptr [EBP + -0x78]
    FILD QWORD PTR [rsp+0]
overlay_00401a3d: ;00401A3D FSUB float ptr [ESI + 0x90]
    FSUB DWORD PTR [R10 + 090h]
overlay_00401a43: ;00401A43 FXCH
    FXCH st(1)
overlay_00401a45: ;00401A45 FCOMPP
    FCOMPP
overlay_00401a47: ;00401A47 FNSTSW AX
    FNSTSW AX
overlay_00401a49: ;00401A49 TEST AH,0x1
    TEST AH,01h
overlay_00401a4c: ;00401A4C JZ 0x00401a93
    JZ overlay_00401a93
overlay_00401a4e: ;00401A4E FLD float ptr [ESI + 0x88]
    FLD DWORD PTR [R10 + 088h]
overlay_00401a54: ;00401A54 FADD float ptr [ESI + 0x84]
    FADD DWORD PTR [R10 + 084h]
overlay_00401a5a: ;00401A5A FST float ptr [ESI + 0x84]
    FST DWORD PTR [R10 + 084h]
overlay_00401a60: ;00401A60 FCOMP float ptr [0x0046e1e0]
    FCOMP DWORD PTR [score_timer]
overlay_00401a66: ;00401A66 FNSTSW AX
    FNSTSW AX
overlay_00401a68: ;00401A68 TEST AH,0x41
    TEST AH,041h
overlay_00401a6b: ;00401A6B JNZ 0x00401a93
    JNZ overlay_00401a93
overlay_00401a6d: ;00401A6D MOV dword ptr [ESI + 0x84],EDI
    MOV DWORD PTR [R10 + 084h],0
overlay_00401a73: ;00401A73 MOV ECX,dword ptr [ESI + 0x9c]
    MOV ECX,DWORD PTR [R10 + 09ch]
    mov DWORD PTR [r11+4],ecx
overlay_00401a79: ;00401A79 FLD float ptr [ESI + 0xcc]
    FLD DWORD PTR [R10 + 0cch]
overlay_00401a7f: ;00401A7F FADD float ptr [ESI + 0x98]
    FADD DWORD PTR [R10 + 098h]
overlay_00401a85: ;00401A85 PUSH ECX
    ;Caller supplies the actual two argument DWORD spill slots.
overlay_00401a86: ;00401A86 PUSH ECX
    ;Caller supplies the actual two argument DWORD spill slots.
overlay_00401a87: ;00401A87 FSTP float ptr [ESP]
    fstp DWORD PTR [r11]
overlay_00401a8a: ;00401A8A CALL 0x00401030
    mov eax,1
    jmp overlay_finished
overlay_00401a8f: ;00401A8F JMP 0x00401a93
    JMP overlay_00401a93
overlay_00401a91: ;00401A91 FSTP ST0
    FSTP st(0)
overlay_00401a93:
    xor eax,eax
overlay_finished:
    add rsp,152
    ret
sgn_source_score_overlay_401a10 ENDP

PUBLIC sgn_source_score_tail_401a93
sgn_source_score_tail_401a93 PROC FRAME
    sub rsp,152
    .allocstack 152
    .endprolog
    lea r10,[rcx+18h]
tail_00401a93: ;00401A93 MOV EDX,dword ptr [ESI + 0x10]
    MOV EDX,DWORD PTR [R10 + 010h]
tail_00401a96: ;00401A96 INC EDX
    INC EDX
tail_00401a97: ;00401A97 MOV dword ptr [ESI + 0x10],EDX
    MOV DWORD PTR [R10 + 010h],EDX
tail_00401a9a: ;00401A9A MOV ECX,dword ptr [ESI + 0x14]
    MOV ECX,DWORD PTR [R10 + 014h]
tail_00401a9d: ;00401A9D MOV EAX,EDX
    MOV EAX,EDX
tail_00401a9f: ;00401A9F CMP EAX,ECX
    CMP EAX,ECX
tail_00401aa1: ;00401AA1 JC 0x00401aa6
    JC tail_00401aa6
tail_00401aa3: ;00401AA3 MOV dword ptr [ESI + -0x18],EDI
    MOV DWORD PTR [R10 + -018h],0
tail_00401aa6: ;00401AA6 FLD float ptr [ESI + 0x48]
    FLD DWORD PTR [R10 + 048h]
tail_00401aa9: ;00401AA9 CALL 0x00462860
    score_ftol
tail_00401aae: ;00401AAE MOV dword ptr [ESI + 0xc4],EAX
    MOV DWORD PTR [R10 + 0c4h],EAX
tail_00401ab4: ;00401AB4 FLD float ptr [ESI + 0x38]
    FLD DWORD PTR [R10 + 038h]
tail_00401ab7: ;00401AB7 FCOMP float ptr [0x0046e1ec]
    FCOMP DWORD PTR [score_zero]
tail_00401abd: ;00401ABD FNSTSW AX
    FNSTSW AX
tail_00401abf: ;00401ABF TEST AH,0x40
    TEST AH,040h
tail_00401ac2: ;00401AC2 JZ 0x00401acb
    JZ tail_00401acb
tail_00401ac4: ;00401AC4 MOV dword ptr [ESI + 0x38],0x3dcccccd
    MOV DWORD PTR [R10 + 038h],03dcccccdh
tail_00401acb: ;00401ACB MOV EDX,dword ptr [ESI + 0x38]
    MOV EDX,DWORD PTR [R10 + 038h]
tail_00401ace: ;00401ACE MOV dword ptr [ESI + 0xd4],EDX
    MOV DWORD PTR [R10 + 0d4h],EDX
tail_00401ad4: ;00401AD4 MOV EAX,dword ptr [ESI + 0x38]
    MOV EAX,DWORD PTR [R10 + 038h]
tail_00401ad7: ;00401AD7 MOV dword ptr [ESI + 0xd8],EAX
    MOV DWORD PTR [R10 + 0d8h],EAX
tail_00401add: ;00401ADD MOV ECX,dword ptr [ESI + 0x58]
    MOV ECX,DWORD PTR [R10 + 058h]
tail_00401ae0: ;00401AE0 MOV dword ptr [ESI + 0xc8],ECX
    MOV DWORD PTR [R10 + 0c8h],ECX
    add rsp,152
    ret
sgn_source_score_tail_401a93 ENDP

PUBLIC sgn_source_score_sparkle_401064
sgn_source_score_sparkle_401064 PROC FRAME
    sub rsp,24
    .allocstack 24
    .endprolog
    mov r10,rcx
    mov r11,rdx
sparkle_00401064: ;00401064 MOV EAX,0x43800000
    MOV EAX,043800000h
sparkle_00401069: ;00401069 MOV dword ptr [ESI + 0xd4],0x2
    MOV DWORD PTR [R10 + 0d4h],02h
sparkle_00401073: ;00401073 MOV dword ptr [ESI + 0xbc],EAX
    MOV DWORD PTR [R10 + 0bch],EAX
sparkle_00401079: ;00401079 MOV dword ptr [ESI + 0xc0],EAX
    MOV DWORD PTR [R10 + 0c0h],EAX
sparkle_0040107f: ;0040107F MOV dword ptr [ESI + 0x10],0x18
    MOV DWORD PTR [R10 + 010h],018h
sparkle_00401086: ;00401086 MOV dword ptr [ESI + 0x2c],0x1e
    MOV DWORD PTR [R10 + 02ch],01eh
sparkle_0040108d: ;0040108D PUSH 0x41700000
    mov DWORD PTR [rsp+4],041700000h
sparkle_00401092: ;00401092 MOV dword ptr [ESI + 0x24],0xbf000000
    MOV DWORD PTR [R10 + 024h],0bf000000h
sparkle_00401099: ;00401099 CALL 0x0042f1c0
    score_random
sparkle_0040109e: ;0040109E FSTP float ptr [ESI + 0x14]
    FSTP DWORD PTR [R10 + 014h]
sparkle_004010a1: ;004010A1 PUSH 0x40f00000
    mov DWORD PTR [rsp+4],040f00000h
sparkle_004010a6: ;004010A6 CALL 0x0042f1c0
    score_random
sparkle_004010ab: ;004010AB FSUB float ptr [0x0046e1dc]
    FSUB DWORD PTR [score_noise_y]
sparkle_004010b1: ;004010B1 FSTP float ptr [ESI + 0x18]
    FSTP DWORD PTR [R10 + 018h]
sparkle_004010b4: ;004010B4 MOV dword ptr [ESI + 0x70],EDI
    MOV DWORD PTR [R10 + 070h],0
sparkle_004010b7: ;004010B7 MOV dword ptr [ESI + 0x64],0x41000000
    MOV DWORD PTR [R10 + 064h],041000000h
sparkle_004010be: ;004010BE MOV dword ptr [ESI + 0x68],EDI
    MOV DWORD PTR [R10 + 068h],0
sparkle_004010c1: ;004010C1 MOV dword ptr [ESI + 0x6c],0x41700000
    MOV DWORD PTR [R10 + 06ch],041700000h
sparkle_004010c8: ;004010C8 MOV dword ptr [ESI + 0x50],EDI
    MOV DWORD PTR [R10 + 050h],0
sparkle_004010cb: ;004010CB MOV dword ptr [ESI + 0x44],0x3dcccccd
    MOV DWORD PTR [R10 + 044h],03dcccccdh
sparkle_004010d2: ;004010D2 MOV dword ptr [ESI + 0x48],0x40c00000
    MOV DWORD PTR [R10 + 048h],040c00000h
sparkle_004010d9: ;004010D9 MOV dword ptr [ESI + 0x4c],0x41700000
    MOV DWORD PTR [R10 + 04ch],041700000h
sparkle_004010e0: ;004010E0 MOV dword ptr [ESI + 0x54],0x437f0000
    MOV DWORD PTR [R10 + 054h],0437f0000h
sparkle_004010e7: ;004010E7 MOV dword ptr [ESI + 0x58],0x41100000
    MOV DWORD PTR [R10 + 058h],041100000h
sparkle_004010ee: ;004010EE MOV dword ptr [ESI + 0x5c],0x41700000
    MOV DWORD PTR [R10 + 05ch],041700000h
sparkle_004010f5: ;004010F5 MOV dword ptr [ESI + 0x8c],0xffffffff
    MOV DWORD PTR [R10 + 08ch],0ffffffffh
sparkle_004010ff: ;004010FF MOV dword ptr [ESI],0x1
    MOV DWORD PTR [R10],01h
sparkle_00401105: ;00401105 MOV dword ptr [ESI + 0x8],EDI
    MOV DWORD PTR [R10 + 08h],0
sparkle_00401108: ;00401108 MOV dword ptr [ESI + 0x28],EDI
    MOV DWORD PTR [R10 + 028h],0
sparkle_0040110b: ;0040110B MOV dword ptr [ESI + 0x40],EDI
    MOV DWORD PTR [R10 + 040h],0
sparkle_0040110e: ;0040110E MOV dword ptr [ESI + 0x98],EDI
    MOV DWORD PTR [R10 + 098h],0
sparkle_00401114: ;00401114 MOV dword ptr [ESI + 0x34],EDI
    MOV DWORD PTR [R10 + 034h],0
sparkle_00401117: ;00401117 MOV dword ptr [ESI + 0xd8],0x58
    MOV DWORD PTR [R10 + 0d8h],058h
sparkle_00401121: ;00401121 MOV EAX,dword ptr [ESI + 0x10]
    MOV EAX,DWORD PTR [R10 + 010h]
sparkle_00401124: ;00401124 MOV dword ptr [ESI + 0xac],EAX
    MOV DWORD PTR [R10 + 0ach],EAX
sparkle_0040112a: ;0040112A MOV dword ptr [ESI + 0xdc],EDI
    MOV DWORD PTR [R10 + 0dch],0
sparkle_00401130: ;00401130 MOV EAX,0x3f800000
    MOV EAX,03f800000h
sparkle_00401135: ;00401135 MOV dword ptr [ESI + 0xe0],EDI
    MOV DWORD PTR [R10 + 0e0h],0
sparkle_0040113b: ;0040113B MOV dword ptr [ESI + 0xec],EAX
    MOV DWORD PTR [R10 + 0ech],EAX
sparkle_00401141: ;00401141 MOV dword ptr [ESI + 0xf0],EAX
    MOV DWORD PTR [R10 + 0f0h],EAX
sparkle_00401147: ;00401147 FLD float ptr [ESI + 0xbc]
    FLD DWORD PTR [R10 + 0bch]
sparkle_0040114d: ;0040114D FMUL float ptr [0x0046e1d8]
    FMUL DWORD PTR [score_half]
sparkle_00401153: ;00401153 FSTP float ptr [ESI + 0xe4]
    FSTP DWORD PTR [R10 + 0e4h]
sparkle_00401159: ;00401159 FLD float ptr [ESI + 0xc0]
    FLD DWORD PTR [R10 + 0c0h]
sparkle_0040115f: ;0040115F FMUL float ptr [0x0046e1d8]
    FMUL DWORD PTR [score_half]
sparkle_00401165: ;00401165 FSTP float ptr [ESI + 0xe8]
    FSTP DWORD PTR [R10 + 0e8h]
sparkle_0040116b: ;0040116B FLD float ptr [0x0046e1d4]
    FLD DWORD PTR [score_screen_x]
sparkle_00401171: ;00401171 FSUB float ptr [ESI + 0xe4]
    FSUB DWORD PTR [R10 + 0e4h]
sparkle_00401177: ;00401177 FSTP float ptr [ESI + 0xb0]
    FSTP DWORD PTR [R10 + 0b0h]
sparkle_0040117d: ;0040117D FLD float ptr [0x0046e1d0]
    FLD DWORD PTR [score_screen_y]
sparkle_00401183: ;00401183 FSUB float ptr [ESI + 0xe8]
    FSUB DWORD PTR [R10 + 0e8h]
sparkle_00401189: ;00401189 FSTP float ptr [ESI + 0xb4]
    FSTP DWORD PTR [R10 + 0b4h]
sparkle_0040118f: ;0040118F FLD float ptr [EBP + 0x8]
    fld DWORD PTR [r8]
sparkle_00401192: ;00401192 MOV dword ptr [ESI + 0xb8],EAX
    MOV DWORD PTR [R10 + 0b8h],EAX
sparkle_00401198: ;00401198 FSUB float ptr [ESI + 0xe4]
    FSUB DWORD PTR [R10 + 0e4h]
sparkle_0040119e: ;0040119E FSTP float ptr [ESI + 0xb0]
    FSTP DWORD PTR [R10 + 0b0h]
sparkle_004011a4: ;004011A4 FLD float ptr [EBP + 0xc]
    fld DWORD PTR [r8+4]
sparkle_004011a7: ;004011A7 FSUB float ptr [ESI + 0xe8]
    FSUB DWORD PTR [R10 + 0e8h]
sparkle_004011ad: ;004011AD FSTP float ptr [ESI + 0xb4]
    FSTP DWORD PTR [R10 + 0b4h]
    add rsp,24
    ret
sgn_source_score_sparkle_401064 ENDP
END
