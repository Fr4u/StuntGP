; Windows x64: RCX is the zero-extended unsigned millisecond difference.
; Preserve the caller's x87 precision/rounding control, as the original does.
; 44E649 FILD qword, 44E657 FMUL m32, 44E669 FSTP m32.
.const
source_frame_scale DWORD 03D75C28Fh ; pinned image 0046EB9C
.code
PUBLIC source_application_frame_scale_x87
source_application_frame_scale_x87 PROC
    sub rsp, 16
    mov dword ptr [rsp], ecx
    mov dword ptr [rsp+4], 0
    fild qword ptr [rsp]
    fmul dword ptr [source_frame_scale]
    fstp dword ptr [rsp+8]
    movss xmm0, dword ptr [rsp+8]
    add rsp, 16
    ret
source_application_frame_scale_x87 ENDP
END
