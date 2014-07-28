;***
;handlers.asm
;
;    Copyright (c) Microsoft Corporation. All rights reserved.
;
;*******************************************************************************

include ksamd64.inc


EXTRN   _GetImageBase:PROC
EXTRN   _NLG_Notify:PROC
EXTRN   __NLG_Return2:PROC

;;++
;;
;;extern "C" void* _CallSettingFrame(
;;    void*               handler,
;;    EHRegistrationNode  *pEstablisher,
;;    ULONG               NLG_CODE)
;;
;;--
 
CsFrame struct
    P1Home          dq      ?               ; Parameter save area
    P2Home          dq      ?               ;
    P3Home          dq      ?               ;
    P4Home          dq      ?               ;
    Alignment       dq      ?               ; Establisher context
    Return          dq      ?               ; Caller's return address
    Handler         dq      ?               ; Caller parameter save area
    Establisher     dq      ?               ;
    NlgCode         dd      ?               ;
CsFrame ends
 
 
    NESTED_ENTRY _CallSettingFrame, _TEXT

    alloc_stack (CsFrame.Return)            ; Allocate private portion of
                                            ; the frame.
    END_PROLOGUE

    mov     CsFrame.Handler[rsp], rcx       ; move parameters to home slots
    mov     CsFrame.Establisher[rsp], rdx   ;
    mov     CsFrame.NlgCode[rsp], r8d       ;

    mov     rdx, [rdx]                      ; dereference establisher pointer
    mov     rax, rcx                        ; prepare return value for dummy notify
    call    _NLG_Notify                     ; notify debugger
    call    rax                             ; call the handler
    call    __NLG_Return2                   ; notify debugger : eh return

    mov     rcx, rax                        ; handler
    mov     rdx, CsFrame.Establisher[rsp]
    mov     rdx, [rdx]                      ; dereference establisher pointer
    mov     r8d, 02h                        ; NLG_CATCH_LEAVE
    call    _NLG_Notify                     ; notify debugger clean the stack,
    add     rsp, CsFrame.Return             ; and return
    ret

    NESTED_END _CallSettingFrame, _TEXT

;;++
;;
;;extern "C"
;;VOID
;;_GetNextInstrOffset (
;;    PVOID* ppReturnPoint
;;    );
;;
;;Routine Description:
;;
;;    This function scans the scope tables associated with the specified
;;    procedure and calls exception and termination handlers as necessary.
;;
;;Arguments:
;;
;;    ppReturnPoint (r32) - store b0 in *pReturnPoint
;;
;;Return Value:
;;
;;  None
;;
;;--

PUBLIC _GetNextInstrOffset
_TEXT SEGMENT
_GetNextInstrOffset PROC

    mov rax, QWORD PTR[rsp]
    mov QWORD PTR [rcx], rax
    ret 0

_GetNextInstrOffset ENDP
_TEXT ENDS

END
