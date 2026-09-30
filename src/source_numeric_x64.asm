; NEW native AMD64 ABI bridge for actual x87 instruction boundaries.
; No executable bytes are copied from the retail PE. Source semantics use
; FSTCW (waiting form), then FLDCW, just as463041 does. Leaf helpers do not
; change RSP or integer nonvolatile registers. [rsp+8] is caller home space.
; These functions explicitly change/read FPCSR by contract, the documented
; exception to ordinary preservation of nonvolatile floating control flags.
OPTION CASEMAP:NONE
.code
PUBLIC sgn_source_read_x87_control
sgn_source_read_x87_control PROC
    fstcw WORD PTR [rsp+8]
    movzx eax, WORD PTR [rsp+8]
    ret
sgn_source_read_x87_control ENDP

PUBLIC sgn_source_write_x87_control
sgn_source_write_x87_control PROC
    mov WORD PTR [rsp+8], cx
    fldcw WORD PTR [rsp+8]
    ret
sgn_source_write_x87_control ENDP
; Source424BD0 and462860: pinned PE4a1d234c, source function ranges and
; instruction hashes are supplied by the private receiving packet. New AMD64
; ABI adaptation; source arithmetic order and six owner stores stay explicit.
; Optional receiving writes are integer-only poststore readbacks, not callbacks.
IFNDEF SGN_VIEWPORT_TEST_BREAK
SGN_VIEWPORT_TEST_BREAK EQU 0
ENDIF
IF SGN_VIEWPORT_TEST_BREAK GT 2
.ERR <Invalid viewport receiving break selector>
ENDIF

VIEWPORT_OBSERVE MACRO sourcepc, fieldoffset, slotindex
    LOCAL no_observer
    test r9,r9
    jz no_observer
    mov ecx,DWORD PTR [r10+fieldoffset]
    mov DWORD PTR [r9+16+slotindex*8],sourcepc
    mov DWORD PTR [r9+20+slotindex*8],ecx
    mov DWORD PTR [r9+64],slotindex+1
no_observer:
ENDM

PUBLIC sgn_source_viewport_424bd0
sgn_source_viewport_424bd0 PROC FRAME
    ; Entry RSP%16=8. 552 bytes gives aligned512-byte FXSAVE at RSP+32.
    sub rsp,552
    .allocstack 552
    .endprolog
    mov r10,rcx                         ; SAME ViewportState
    mov r11,r9                          ; borrowed NativeCall
    mov DWORD PTR [rsp],edx             ; raw X argument, no conversion
    mov DWORD PTR [rsp+4],r8d            ; local Y argument, later source FSTP
    fxsave [rsp+32]                     ; NEW nonwaiting, nonmutating admission
    movzx eax,WORD PTR [rsp+32]
    mov WORD PTR [r11+2],ax
    mov DWORD PTR [r11+4],1              ; SEH finalizer now has entry CW
    cmp ax,WORD PTR [r11]
    jne vp_reject_control
    mov ecx,eax
    and ecx,03fh
    cmp ecx,03fh
    jne vp_reject_mask
    and eax,0300h
    cmp eax,0100h
    je vp_reject_precision
    cmp BYTE PTR [rsp+36],0             ; FXSAVE abridged tag, all empty
    jne vp_reject_stack
    movzx eax,WORD PTR [rsp+34]
    and eax,03800h
    jnz vp_reject_top
    mov r9,QWORD PTR [r11+8]            ; optional NEW observer
    test r9,r9
    jz vp_source_begin
    mov DWORD PTR [r9+64],0
    mov ax,WORD PTR [rsp+32]
    mov WORD PTR [r9+68],ax
    mov ax,WORD PTR [rsp+34]
    mov WORD PTR [r9+72],ax
    mov eax,DWORD PTR [rsp+56]
    mov DWORD PTR [r9+76],eax
vp_source_begin:
    fld DWORD PTR [r10+24]              ;424BD3 scaleX
    fadd DWORD PTR [rsp]               ;424BD9 X+scaleX in current PC/RC
    fld DWORD PTR [r10+28]              ;424BDC scaleY
    mov ecx,DWORD PTR [rsp+4]           ;424BE2 original Y bits
    mov eax,DWORD PTR [rsp]             ;424BE5 original X bits
    fadd DWORD PTR [rsp+4]              ;424BE8 Y+scaleY in current PC/RC
    mov DWORD PTR [r10+80],eax          ;424BEB originX, first owner store
    ; Observer uses ECX; preserve original Y until its source store below.
    VIEWPORT_OBSERVE 0424bebh,80,0
    mov ecx,DWORD PTR [rsp+4]
    mov DWORD PTR [r10+84],ecx          ;424BF0 originY
    VIEWPORT_OBSERVE 0424bf0h,84,1
    fstp DWORD PTR [rsp+4]              ;424BF6 round/store Y first
    fld st(0)                          ;424BF9 duplicate UNROUNDED X
    ;424BFB ->462860, first distinct conversion, including extra WAIT.
    fstcw WORD PTR [rsp+8]              ;462866 includes waiting prefix
    fwait                              ;46286A extra source WAIT
    mov ax,WORD PTR [rsp+8]
    or ah,0ch                          ;46286F only RC becomes truncate
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
IF SGN_VIEWPORT_TEST_BREAK EQ 1
    int 3                              ; receiving-only abnormal unwind probe
ENDIF
    fistp QWORD PTR [rsp+16]            ;462879 actual signed64 conversion
    fldcw WORD PTR [rsp+8]              ;46287C restore THIS conversion CW
    mov eax,DWORD PTR [rsp+16]
    mov edx,DWORD PTR [rsp+20]
    test r9,r9
    jz vp_after_x_conversion
    mov r8,QWORD PTR [rsp+16]
    mov QWORD PTR [r9],r8
vp_after_x_conversion:
    fld DWORD PTR [rsp+4]               ;424C00 Y reload BEFORE X owner store
    mov DWORD PTR [r10+8],eax           ;424C03 X low32, no saturation
    VIEWPORT_OBSERVE 0424c03h,8,2
    ;424C08 ->462860, second distinct conversion and own CW save/restore.
    fstcw WORD PTR [rsp+8]
    fwait
    mov ax,WORD PTR [rsp+8]
    or ah,0ch
    mov WORD PTR [rsp+10],ax
    fldcw WORD PTR [rsp+10]
IF SGN_VIEWPORT_TEST_BREAK EQ 2
    int 3                              ; receiving-only abnormal unwind probe
ENDIF
    fistp QWORD PTR [rsp+16]
    fldcw WORD PTR [rsp+8]
    mov eax,DWORD PTR [rsp+16]
    mov edx,DWORD PTR [rsp+20]
    test r9,r9
    jz vp_after_y_conversion
    mov r8,QWORD PTR [rsp+16]
    mov QWORD PTR [r9+8],r8
vp_after_y_conversion:
    mov edx,DWORD PTR [rsp+4]           ;424C0D retained binary32 Y
    mov DWORD PTR [r10+12],eax          ;424C10 Y low32
    VIEWPORT_OBSERVE 0424c10h,12,3
    fstp DWORD PTR [r10+16]             ;424C15 X binary32, DIRECT owner store
    VIEWPORT_OBSERVE 0424c15h,16,4
    mov DWORD PTR [r10+20],edx          ;424C1B Y binary32
    VIEWPORT_OBSERVE 0424c1bh,20,5
    test r9,r9
    jz vp_accepted
    fnstcw WORD PTR [r9+70]
    fnstsw WORD PTR [r9+74]
    stmxcsr DWORD PTR [r9+80]           ; observer only, no MXCSR changes
vp_accepted:
    xor eax,eax
    jmp vp_return
vp_reject_control:
    mov eax,1
    jmp vp_return
vp_reject_mask:
    mov eax,2
    jmp vp_return
vp_reject_precision:
    mov eax,3
    jmp vp_return
vp_reject_stack:
    mov eax,4
    jmp vp_return
vp_reject_top:
    mov eax,5
vp_return:
    add rsp,552
    ret
sgn_source_viewport_424bd0 ENDP
END
