/***
*gs_report.c - Report a /GS security check failure
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines function used to report a security check failure.
*       This file corresponds to gs_report.c in the Windows sources.
*
*       Entrypoints:
*            __report_gsfailure
*
*******************************************************************************/

#include <windows.h>

#if defined (_CRTBLD) && !defined (_SYSCRT)
#include <dbgint.h>     /* needed for _CRT_DEBUGGER_HOOK */
#include <awint.h>      /* needed for Win32 API helpers */
#endif  /* defined (_CRTBLD) && !defined (_SYSCRT) */

extern UINT_PTR __security_cookie;
extern UINT_PTR __security_cookie_complement;

/*
 * Use global memory for the exception and context records, and any other local
 * state needed by __report_gsfailure and __report_securityfailure.  We're not
 * worried about multithread issues here - we're going to die anyway, and it
 * saves us from consuming a good chunk of stack, as well as potentially
 * overwriting useful data in the stack memory dump.
 */

static EXCEPTION_RECORD         GS_ExceptionRecord;
static CONTEXT                  GS_ContextRecord;
static const EXCEPTION_POINTERS GS_ExceptionPointers = {
    &GS_ExceptionRecord,
    &GS_ContextRecord
};

#if defined (_CRTBLD) && !defined (_SYSCRT)
static BOOL DebuggerWasPresent;
#endif  /* defined (_CRTBLD) && !defined (_SYSCRT) */

#define STATUS_SECURITY_CHECK_FAILURE STATUS_STACK_BUFFER_OVERRUN

/***
*__raise_securityfailure() - Raises a security failure and terminates the process.
*
*Purpose:
*       Invokes the unhandled exception filter using the provided exception
*       pointers and then terminate the process.
*
*       This function is not defined for ARM, and is not called on Win8.
*       In ARM and Win8, __fastfail() will be called instead.
*
*Exit:
*       Does not return.
*
*******************************************************************************/
#if !defined (_CRT_APP) && (defined (_M_IX86) || defined (_M_X64))
__declspec(noreturn) void __cdecl __raise_securityfailure(
    PEXCEPTION_POINTERS ExceptionPointers
    )
{
#if defined (_CRTBLD) && !defined (_SYSCRT)
    DebuggerWasPresent = IsDebuggerPresent();
    _CRT_DEBUGGER_HOOK(_CRT_DEBUGGER_GSFAILURE);
#endif  /* defined (_CRTBLD) && !defined (_SYSCRT) */

    __crtUnhandledException(ExceptionPointers);

#if defined (_CRTBLD) && !defined (_SYSCRT)
    /*
     * If we make it back from Watson, then the user may have asked to debug
     * the app.  If we weren't under a debugger before invoking Watson,
     * re-signal the VS CRT debugger hook, so a newly attached debugger gets
     * a chance to break into the process.
     */
    if (!DebuggerWasPresent)
    {
        _CRT_DEBUGGER_HOOK(_CRT_DEBUGGER_GSFAILURE);
    }
#endif  /* defined (_CRTBLD) && !defined (_SYSCRT) */

    __crtTerminateProcess(STATUS_SECURITY_CHECK_FAILURE);
}
#endif  /* !defined (_CRT_APP) && (defined (_M_IX86) || defined (_M_X64)) */

#pragma optimize("", off)   /* force an EBP frame on x86, no stack packing */
/* Forcing a frame applies to all the functions below ie __report_gsfailure,
 * __report_securityfailure, and __report_securityfailureEx
 */

/***
*__report_gsfailure() - Report security error.
*
*Purpose:
*       A /GS security error has been detected.  We save the registers in a
*       CONTEXT struct & call UnhandledExceptionFilter to display a message
*       to the user (invoke Watson handling) and terminate the program.
*
*       NOTE: __report_gsfailure should not be called directly.  Instead, it
*       should be entered due to a failure detected by __security_check_cookie.
*       That's because __security_check_cookie may perform more checks than
*       just a mismatch against the global security cookie, and because
*       the context captured by __report_gsfailure is unwound assuming that
*       __security_check_cookie was used to detect the failure.
*
*Entry:
*       ULONGLONG StackCookie - the local cookie, passed in as actual argument
*       on AMD64 only.  On x86, the local cookie is available in ECX,
*       but since __report_gsfailure isn't __fastcall, it isn't a true
*       argument, and we must flush ECX to the context record quickly.
*
*Exit:
*       Does not return.
*
*Exceptions:
*
*******************************************************************************/

#if defined (_M_IX86)
__declspec(noreturn) void __cdecl __report_gsfailure(void)
#elif defined (_M_ARM)
__declspec(noreturn) void __cdecl __report_gsfailure(uintptr_t StackCookie)
#else  /* defined (_M_ARM) */
__declspec(noreturn) void __cdecl __report_gsfailure(ULONGLONG StackCookie)
#endif  /* defined (_M_ARM) */
{
#if defined (_CRT_APP)
        __fastfail(FAST_FAIL_STACK_COOKIE_CHECK_FAILURE);
#else  /* defined (_CRT_APP) */

#if defined (_M_IX86) || defined (_M_X64)
    volatile UINT_PTR cookie[2];
#if defined (_M_IX86)
    /*
     * On x86, we reserve some extra stack which won't be used.  That is to
     * preserve as much of the call frame as possible when the function with
     * the buffer overrun entered __security_check_cookie with a JMP instead of
     * a CALL, after the calling frame has been released in the epilogue of
     * that function.
     */
    volatile ULONG dw[(sizeof(CONTEXT) + sizeof(EXCEPTION_RECORD))/sizeof(ULONG)];
#endif  /* defined (_M_IX86) */
#endif  /* defined (_M_IX86) || defined (_M_X64) */

#if defined (_M_IX86) || defined (_M_X64)
    if (IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE))
#endif  /* defined (_M_IX86) || defined (_M_X64) */
        __fastfail(FAST_FAIL_STACK_COOKIE_CHECK_FAILURE);

#if defined (_M_IX86) || defined (_M_X64)
    /*
     * Set up a fake exception, and report it via UnhandledExceptionFilter.
     * We can't raise a true exception because the stack (and therefore
     * exception handling) can't be trusted after a buffer overrun.  The
     * exception should appear as if it originated after the call to
     * __security_check_cookie, so it is attributed to the function where the
     * buffer overrun was detected.
     */

#if defined (_M_IX86)
    /*
     * Save the state in the context record immediately.  Hopefully, since
     * opts are disabled, this will happen without modifying ECX, which has
     * the local cookie which failed the check.
     */

    __asm {
        mov dword ptr [GS_ContextRecord.Eax], eax
        mov dword ptr [GS_ContextRecord.Ecx], ecx
        mov dword ptr [GS_ContextRecord.Edx], edx
        mov dword ptr [GS_ContextRecord.Ebx], ebx
        mov dword ptr [GS_ContextRecord.Esi], esi
        mov dword ptr [GS_ContextRecord.Edi], edi
        mov word ptr [GS_ContextRecord.SegSs], ss
        mov word ptr [GS_ContextRecord.SegCs], cs
        mov word ptr [GS_ContextRecord.SegDs], ds
        mov word ptr [GS_ContextRecord.SegEs], es
        mov word ptr [GS_ContextRecord.SegFs], fs
        mov word ptr [GS_ContextRecord.SegGs], gs
        pushfd
        pop [GS_ContextRecord.EFlags]

        /*
         * Set the context EBP/EIP/ESP to the values which would be found
         * in the caller to __security_check_cookie.
         */

        mov eax, [ebp]
        mov dword ptr [GS_ContextRecord.Ebp], eax
        mov eax, [ebp+4]
        mov dword ptr [GS_ContextRecord.Eip], eax
        lea eax, [ebp+8]
        mov dword ptr [GS_ContextRecord.Esp], eax

        /*
         * Make sure the dummy stack space looks referenced.
         */

        mov eax, dword ptr dw
    }

    GS_ContextRecord.ContextFlags = CONTEXT_CONTROL;
    GS_ExceptionRecord.ExceptionAddress  = (PVOID)(ULONG_PTR)GS_ContextRecord.Eip;

#elif defined (_M_X64)

    __crtCapturePreviousContext(&GS_ContextRecord); /* Capture the preceding frame context */

    GS_ContextRecord.Rip = (ULONGLONG) _ReturnAddress();
    GS_ContextRecord.Rsp = (ULONGLONG) _AddressOfReturnAddress()+8;
    GS_ExceptionRecord.ExceptionAddress = (PVOID)GS_ContextRecord.Rip;
    GS_ContextRecord.Rcx = StackCookie;
#endif  /* defined (_M_X64) */

    GS_ExceptionRecord.ExceptionCode  = STATUS_SECURITY_CHECK_FAILURE;
    GS_ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    GS_ExceptionRecord.NumberParameters = 1;
    GS_ExceptionRecord.ExceptionInformation[0] = FAST_FAIL_STACK_COOKIE_CHECK_FAILURE;

    /*
     * Save the global cookie and cookie complement locally - using an array
     * to defeat any potential stack-packing.
     */

    cookie[0] = __security_cookie;
    cookie[1] = __security_cookie_complement;

    /*
     * Raise the security failure by passing it to the unhandled exception
     * filter and then terminate the process.
     */

    __raise_securityfailure((EXCEPTION_POINTERS *)&GS_ExceptionPointers);

#endif  /* defined (_M_IX86) || defined (_M_X64) */

#endif  /* defined (_CRT_APP) */
}

/***
*__report_securityfailureEx() - Report a specific security failure condition.
*
*Purpose:
*       Reports a specific security failure condition.  The type of failure that
*       occurred is embodied in the FailureCode that is provided as a parameter.
*       A specific failure condition can optionally specify additional parameters
*       that should be recorded as part of the exception that is generated.
*
*       NOTE: Unlike __report_gsfailure, __report_securityfailureEx assumes that
*       it is called directly by the function in which the failure occurred.
*
*Exit:
*       Does not return.
*
*******************************************************************************/
__declspec(noreturn) void __cdecl __report_securityfailureEx(
    _In_ ULONG FailureCode,
    _In_ ULONG NumberParameters,
    _In_reads_opt_(NumberParameters) void **Parameters
    )
{
#if defined (_CRT_APP)
        __fastfail(FailureCode);
#else  /* defined (_CRT_APP) */

#if defined (_M_IX86) || defined (_M_X64)
    ULONG i;
#if defined (_M_IX86)
    volatile ULONG dw[(sizeof(CONTEXT) + sizeof(EXCEPTION_RECORD))/sizeof(ULONG)];
#endif  /* defined (_M_IX86) */
#endif  /* defined (_M_IX86) || defined (_M_X64) */

#if defined (_M_IX86) || defined (_M_X64)
    if (IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE))
#endif  /* defined (_M_IX86) || defined (_M_X64) */
        __fastfail(FailureCode);

#if defined (_M_IX86) || defined (_M_X64)

    /*
     * Set up a fake exception, and report it via UnhandledExceptionFilter.
     * We can't raise a true exception because the stack (and therefore
     * exception handling) can't be trusted.  The exception should appear as
     * if it originated after the call to __report_securityfailureEx, so it is
     * attributed to the function where the violation occurred.
     *
     * We assume that the immediate caller of __report_securityfailureEx is
     * the function where the security violation occurred.  Note that the compiler
     * may elect to emit a jump to this routine instead of a call, in which case
     * we will not be able to blame the correct function.
     */


#if defined (_M_IX86)

    /*
     * Save the state in the context record immediately.  Hopefully, since
     * opts are disabled, this will happen without modifying ECX, which has
     * the local cookie which failed the check.
     */

    __asm {
        mov dword ptr [GS_ContextRecord.Eax], eax
        mov dword ptr [GS_ContextRecord.Ecx], ecx
        mov dword ptr [GS_ContextRecord.Edx], edx
        mov dword ptr [GS_ContextRecord.Ebx], ebx
        mov dword ptr [GS_ContextRecord.Esi], esi
        mov dword ptr [GS_ContextRecord.Edi], edi
        mov word ptr [GS_ContextRecord.SegSs], ss
        mov word ptr [GS_ContextRecord.SegCs], cs
        mov word ptr [GS_ContextRecord.SegDs], ds
        mov word ptr [GS_ContextRecord.SegEs], es
        mov word ptr [GS_ContextRecord.SegFs], fs
        mov word ptr [GS_ContextRecord.SegGs], gs
        pushfd
        pop [GS_ContextRecord.EFlags]

        /*
         * Set the context EBP/EIP/ESP to the values which would be found
         * in the caller to __security_check_cookie.
         */

        mov eax, [ebp]
        mov dword ptr [GS_ContextRecord.Ebp], eax
        mov eax, [ebp+4]
        mov dword ptr [GS_ContextRecord.Eip], eax
        lea eax, [ebp+8]
        mov dword ptr [GS_ContextRecord.Esp], eax

        /*
         * Make sure the dummy stack space looks referenced.
         */

        mov eax, dword ptr dw
    }

    GS_ExceptionRecord.ExceptionAddress  = (PVOID)GS_ContextRecord.Eip;
#elif defined (_M_X64)
    __crtCaptureCurrentContext(&GS_ContextRecord);
    GS_ContextRecord.Rip = (ULONGLONG) _ReturnAddress();
    GS_ContextRecord.Rsp = (ULONGLONG) _AddressOfReturnAddress()+8;
    GS_ExceptionRecord.ExceptionAddress = (PVOID)GS_ContextRecord.Rip;
#endif  /* defined (_M_X64) */

    GS_ExceptionRecord.ExceptionCode  = STATUS_SECURITY_CHECK_FAILURE;
    GS_ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;

    if (NumberParameters > 0 && Parameters == NULL) {
        NumberParameters = 0;
    }

    if (NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS - 1) {
        NumberParameters--;
    }

    GS_ExceptionRecord.NumberParameters = NumberParameters + 1;
    GS_ExceptionRecord.ExceptionInformation[0] = FailureCode;

    for (i = 0; i < NumberParameters; i++) {
        GS_ExceptionRecord.ExceptionInformation[i + 1] = (ULONG_PTR)Parameters[i];
    }

    /*
     * Raise the security failure by passing it to the unhandled exception
     * filter and then terminate the process.
     */

    __raise_securityfailure((EXCEPTION_POINTERS *)&GS_ExceptionPointers);

#endif  /* defined (_M_IX86) || defined (_M_X64) */

#endif  /* defined (_CRT_APP) */
}

/***
*__report_securityfailure() - Report a specific security failure condition.
*
*Purpose:
*       Reports a specific security failure condition.  The type of failure that occurred
*       is embodied in the FailureCode that is provided as a parameter.
*       If a failure condition needs to specify additional parameters then it should call
*       __report_securityfailureEx instead.
*
*       NOTE: Unlike __report_gsfailure, __report_securityfailure assumes that
*       it is called directly by the function in which the failure occurred.
*       This also means that __security_reportfailure doesn't simply call
*          __report_securityfailureEx(FailureCode, 0, 0);
*       as that would alter the call stack.
*
*Exit:
*       Does not return.
*
*******************************************************************************/
__declspec(noreturn) void __cdecl __report_securityfailure(ULONG FailureCode)
{
#if defined (_CRT_APP)
        __fastfail(FailureCode);
#else  /* defined (_CRT_APP) */
#if defined (_M_IX86)
    volatile ULONG dw[(sizeof(CONTEXT) + sizeof(EXCEPTION_RECORD))/sizeof(ULONG)];
#endif  /* defined (_M_IX86) */

#if defined (_M_IX86) || defined (_M_X64)
    if (IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE))
#endif  /* defined (_M_IX86) || defined (_M_X64) */
        __fastfail(FailureCode);

#if defined (_M_IX86) || defined (_M_X64)
    /*
     * Set up a fake exception, and report it via UnhandledExceptionFilter.
     * We can't raise a true exception because the stack (and therefore
     * exception handling) can't be trusted.  The exception should appear as
     * if it originated after the call to __report_securityfailure, so it is
     * attributed to the function where the violation occurred.
     *
     * We assume that the immediate caller of __report_securityfailure is
     * the function where the security violation occurred.  Note that the compiler
     * may elect to emit a jump to this routine instead of a call, in which case
     * we will not be able to blame the correct function.
     */

#if defined (_M_IX86)
    /*
     * Save the state in the context record immediately.  Hopefully, since
     * opts are disabled, this will happen without modifying ECX, which has
     * the local cookie which failed the check.
     */

    __asm {
        mov dword ptr [GS_ContextRecord.Eax], eax
        mov dword ptr [GS_ContextRecord.Ecx], ecx
        mov dword ptr [GS_ContextRecord.Edx], edx
        mov dword ptr [GS_ContextRecord.Ebx], ebx
        mov dword ptr [GS_ContextRecord.Esi], esi
        mov dword ptr [GS_ContextRecord.Edi], edi
        mov word ptr [GS_ContextRecord.SegSs], ss
        mov word ptr [GS_ContextRecord.SegCs], cs
        mov word ptr [GS_ContextRecord.SegDs], ds
        mov word ptr [GS_ContextRecord.SegEs], es
        mov word ptr [GS_ContextRecord.SegFs], fs
        mov word ptr [GS_ContextRecord.SegGs], gs
        pushfd
        pop [GS_ContextRecord.EFlags]

        /*
         * Set the context EBP/EIP/ESP to the values which would be found
         * in the caller to __security_check_cookie.
         */

        mov eax, [ebp]
        mov dword ptr [GS_ContextRecord.Ebp], eax
        mov eax, [ebp+4]
        mov dword ptr [GS_ContextRecord.Eip], eax
        lea eax, [ebp+8]
        mov dword ptr [GS_ContextRecord.Esp], eax

        /*
         * Make sure the dummy stack space looks referenced.
         */

        mov eax, dword ptr dw
    }

    GS_ExceptionRecord.ExceptionAddress  = (PVOID)GS_ContextRecord.Eip;
#elif defined (_M_X64)
    __crtCaptureCurrentContext(&GS_ContextRecord);
    GS_ContextRecord.Rip = (ULONGLONG) _ReturnAddress();
    GS_ContextRecord.Rsp = (ULONGLONG) _AddressOfReturnAddress()+8;
    GS_ExceptionRecord.ExceptionAddress = (PVOID)GS_ContextRecord.Rip;
#endif  /* defined (_M_X64) */

    GS_ExceptionRecord.ExceptionCode  = STATUS_SECURITY_CHECK_FAILURE;
    GS_ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;

    GS_ExceptionRecord.NumberParameters = 1;
    GS_ExceptionRecord.ExceptionInformation[0] = FailureCode;

    /*
     * Raise the security failure by passing it to the unhandled exception
     * filter and then terminate the process.
     */

    __raise_securityfailure((EXCEPTION_POINTERS *)&GS_ExceptionPointers);

#endif  /* defined (_M_IX86) || defined (_M_X64) */
#endif  /* defined (_CRT_APP) */
}

// Declare stub for rangecheckfailure, since these occur often enough that the code bloat
// of setting up the parameters hurts performance

__declspec(noreturn) void __cdecl __report_rangecheckfailure(void)
{
    __report_securityfailure(FAST_FAIL_RANGE_CHECK_FAILURE);
}
