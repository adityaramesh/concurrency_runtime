        TTL ARM /GS cookie check compiler helper
;++
;
; Copyright (c) 2005  Microsoft Corporation
;
; Module Name:
;
;    armsecgs.asm
;
; Abstract:
;
;    ARM security cookie checking routine (derived from CE)
;
; Purpose:
;       ARM security cookie checking routine
;
; void __security_check_cookie(DWORD_PTR cookie)
; {
;    /* Immediately return if the local cookie is OK. */
;    if (cookie == __security_cookie)
;        return;
;
;    /* Report the failure */
;    __report_gsfailure();
; }
;

#include "ksarm.h"

        TEXTAREA

        IMPORT __report_gsfailure
        IMPORT __security_cookie
        IMPORT __security_cookie_complement

DEFAULT_SECURITY_COOKIE     EQU 0x0000B064

        TEXTAREA

; void __security_check_cookie(DWORD_PTR cookie)
;
        LEAF_ENTRY __security_check_cookie

        ldr     r12, =__security_cookie
        ldr     r12, [r12]
        cmp     r0, r12
        bne     __gsfailure
        EPILOG_RETURN

        LEAF_END __security_check_cookie


; void __gsfailure(DWORD_PTR cookie)
;
        NESTED_ENTRY __gsfailure
        PROLOG_PUSH         {r3, lr}
        PROLOG_STACK_ALLOC  8
        
        ; copy the globals to the stack so even minimal dumps get them
        ldr     r12, =__security_cookie_complement
        ldr     r12, [r12]
        str     r12, [sp, #4]
        ldr     r12, =__security_cookie
        ldr     r12, [r12]
        str     r12, [sp]

        ; call the handler keeping the cookie in r0
        bl       __report_gsfailure

        ; should never get here
        EMIT_BREAKPOINT

        NESTED_END __gsfailure

        END

