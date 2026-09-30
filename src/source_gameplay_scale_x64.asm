; NEW Windows AMD64 ABI transport for the actual source arithmetic sequence.
; RCX -> binary64 returned by4631A7, RDX -> retained rule.scale_bits,
; R8 -> actual GameplayRules.values[index]. No intermediate float32 store.
; The existing source caller429510 owns FPU preparation. This leaf neither
; reads nor changes CW/MXCSR, preserves integer nonvolatile registers/RSP,
; and returns with the same x87 stack depth. Status comes from real x87.
; Pinned PE4a1d234c:4631EC FLD,445773 FMUL,445779 FSTP.
OPTION CASEMAP:NONE
.code
PUBLIC sgn_source_gameplay_multiply_store_x87
sgn_source_gameplay_multiply_store_x87 PROC
    fld QWORD PTR [rcx]
    fmul DWORD PTR [rdx]
    fstp DWORD PTR [r8]
    ret
sgn_source_gameplay_multiply_store_x87 ENDP
END
