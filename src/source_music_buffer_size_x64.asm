; Win64 ABI. Exact x87 arithmetic order, no SSE conversion or signed32 clamp.
; Requires three available x87 stack slots and valid fields. No FNINIT/FNCLEX:
; inherited precision, rounding and exception state remain actual inputs.
; No calls and no nonvolatile GPR/XMM clobbers. Restores CW on ordinary return.
OPTION CASEMAP:NONE
.const
music_scale_46ec68 DWORD 03a83126fh
music_thousand_46e2c0 DWORD 0447a0000h
.code
PUBLIC source_music_buffer_size_x64
source_music_buffer_size_x64 PROC FRAME
    sub rsp,48h
    .allocstack 48h
    .endprolog
    mov r10,rcx
    mov r11,QWORD PTR [r10]
    mov eax,DWORD PTR [r11]       ;45FE6D original period
    mov r8d,eax
    shr r8d,2
    mov r11,QWORD PTR [r10+24]
    mov DWORD PTR [r11],r8d      ;45FE7B quarter store before FILD
    mov DWORD PTR [rsp],eax
    mov DWORD PTR [rsp+4],0      ;EDI=0 from45FDAC, callee-preserved
    mov r11,QWORD PTR [r10+8]
    mov eax,DWORD PTR [r11]
    mov DWORD PTR [rsp+8],eax
    mov DWORD PTR [rsp+12],0
    fild QWORD PTR [rsp+8]       ;45FE87 unsigned DWORD extended by known zero
    fild QWORD PTR [rsp]         ;45FE90 period
    fld st(0)
    fmul st(0),st(2)
    fmul DWORD PTR [music_scale_46ec68]
    ; Inline462860: original FISTP is QWORD, return EAX is its LOW DWORD.
    fstcw WORD PTR [rsp+16]
    fwait
    mov ax,WORD PTR [rsp+16]
    or ah,0ch
    mov WORD PTR [rsp+18],ax
    fldcw WORD PTR [rsp+18]
    fistp QWORD PTR [rsp+24]
    fldcw WORD PTR [rsp+16]
    mov r9d,DWORD PTR [rsp+24]
    fld DWORD PTR [music_thousand_46e2c0]
    fdiv st(0),st(1)
    mov ecx,DWORD PTR [r10+56]
    and ecx,0ffh                ;45FEAC discards unknown EBX high24
    mov r11,QWORD PTR [r10+16]
    imul r9d,DWORD PTR [r11]
    fdivr st(0),st(2)
    mov r11,QWORD PTR [r10+32]
    mov DWORD PTR [r11],r9d      ;45FEBC buffer bytes after FDIVR
    mov edx,r9d
    shr edx,cl
    add edx,1
    mov DWORD PTR [rsp+32],edx
    mov r11,QWORD PTR [r10+40]
    mov DWORD PTR [r11],edx      ;45FEC7 sector count before second conversion
    fstcw WORD PTR [rsp+16]
    fwait
    mov ax,WORD PTR [rsp+16]
    or ah,0ch
    mov WORD PTR [rsp+18],ax
    fldcw WORD PTR [rsp+18]
    fistp QWORD PTR [rsp+24]
    fldcw WORD PTR [rsp+16]
    mov eax,DWORD PTR [rsp+24]
    mov edx,1
    shl edx,cl
    fstp st(0)
    fstp st(0)
    imul edx,DWORD PTR [rsp+32]
    cmp edx,eax                 ;45FEE0 unsigned compare, both wrap at32 bits
    jb music_size_insufficient
    mov r11,QWORD PTR [r10]
    add r8d,DWORD PTR [r11]      ;45FEFD reload, add saved quarter with wrap
    mov r11,QWORD PTR [r10+8]
    mov eax,10624dd3h
    mul DWORD PTR [r11]         ;45FF0A exact unsigned high product
    shr edx,6
    imul r8d,edx
    mov r11,QWORD PTR [r10+48]
    mov DWORD PTR [r11],r8d      ;45FF2A stop threshold
    mov eax,1
    add rsp,48h
    ret
music_size_insufficient:
    xor eax,eax
    add rsp,48h
    ret
source_music_buffer_size_x64 ENDP
END
