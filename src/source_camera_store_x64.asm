; Win64 RCX points to the actual4631EC binary64 return bits; RDX to destination.
; Exactly the source FLD qword / FSTP dword, inheriting the current x87 control.
; No calls, stack mutation, nonvolatile registers or MXCSR conversion.
OPTION CASEMAP:NONE
.code
PUBLIC sgn_source_camera_store_4349a2
sgn_source_camera_store_4349a2 PROC
    fld QWORD PTR [rcx]
    fstp DWORD PTR [rdx]
    ret
sgn_source_camera_store_4349a2 ENDP
END
