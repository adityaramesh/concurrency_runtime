/***
*winapisupp.c - helper functions for Windows Apis
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains functions to work with Windows API
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <Windows.h>
#include <AppModel.h>
#include <awint.h>

/*
 * GetCurrentPackageId retrieves the current package id, if the app is deployed via a package.
 * ARM has this API on all Windows versions.
 *
 */
#if defined (_M_IX86) || defined (_M_X64) || defined (_CORESYS) || defined (_KERNELX)
typedef BOOL (WINAPI *PFN_GetCurrentPackageId)(UINT32*, BYTE*);
#endif  /* defined (_M_IX86) || defined (_M_X64) || defined (_CORESYS) || defined (_KERNELX) */

#if !defined(_M_ARM) && (defined (_CORESYS) || defined (_KERNELX))

// Copy the prototype for the dependent function from "kernel32packagecurrentext.h"

BOOL
WINAPI
IsGetCurrentPackageIdPresent(
    VOID
    );

static int __IsPackagedAppHelper(void)
{
    LONG retValue = APPMODEL_ERROR_NO_PACKAGE;
    UINT32 bufferLength = 0;

    PFN_GetCurrentPackageId pfn = NULL;


    // Ensure that GetCurrentPackageId is exported by the dll. This allows breaking changes to result
    // in a build break rather than a silent failure at runtime. Note that this function is implemented
    // in the import lib. That is, we do not have an implicit load dependency on the
    // ext-ms-win-kerne32-package-current-l1-1-0.dll.
    if (IsGetCurrentPackageIdPresent())
    {
        // This dll need not be present on all systems
        HMODULE hPackageDll = LoadLibraryExW(L"ext-ms-win-kernel32-package-current-l1-1-0.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (hPackageDll)
        {
            if (pfn = (PFN_GetCurrentPackageId)(GetProcAddress(hPackageDll, "GetCurrentPackageId")))
            {
                retValue = pfn(&bufferLength, NULL);

                if (retValue == ERROR_INSUFFICIENT_BUFFER)
                {
                    // This is a packaged application
                    FreeLibrary(hPackageDll);
                    return 1;
                }
            }

            FreeLibrary(hPackageDll);
        }
    }

    // Either the app is not packaged or we cannot determine if the app is packaged
    // Return false by default.
    return 0;
}

#else  /* !defined(_M_ARM) && (defined (_CORESYS) || defined (_KERNELX)) */

static int __IsPackagedAppHelper(void)
{
    LONG retValue = APPMODEL_ERROR_NO_PACKAGE;
    UINT32 bufferLength = 0;

#if defined (_M_IX86) || defined (_M_X64)
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFN_GetCurrentPackageId, GetCurrentPackageId, pfn) \
    {
        retValue = pfn(&bufferLength, NULL);
    }
#else  /* defined (_M_IX86) || defined (_M_X64) */
    retValue = GetCurrentPackageId(&bufferLength, NULL);
#endif  /* defined (_M_IX86) || defined (_M_X64) */

    if (retValue == ERROR_INSUFFICIENT_BUFFER)
    {
        return 1;
    }

    /* If GetCurrentPackageId was not found, or it returned a different error,
       then this is NOT a Packaged app */
    return 0;
}
#endif  /* defined (_CORESYS) || defined (_KERNELX) */

/*******************************************************************************
*__crtIsPackagedApp() - Check if the current app is a Packaged app
*
*Purpose:
*       Check if the current application was started through a package
*       This determines if the app is a Packaged app or not.
*
*       This function uses a new Windows 8 API, GetCurrentPackageId, to detect
*       if the application is deployed via a package.
*
*Entry:
*       None
*
*Exit:
*       TRUE if Packaged app, FALSE if not.
*
*Exceptions:
*
*******************************************************************************/
_CRTIMP BOOL __cdecl __crtIsPackagedApp (
        void
        )
{
#ifdef _CRT_APP
    return TRUE;
#else  /* _CRT_APP */
    static int isPackaged = -1; /* initialize to undefined state */

    /* If we've already made this check, just return the prev result */
    if (isPackaged < 0)
    {
        isPackaged = __IsPackagedAppHelper();
    }

    return (isPackaged > 0) ? TRUE : FALSE;
#endif  /* _CRT_APP */
}

#ifndef _CRT_APP

/*******************************************************************************
*__crtGetShowWindowMode() - Return the show window mode in a window app.
*
*Purpose:
*       Check if startup info, wShowWindow member contains additional information
*       and if so, returns wShowWindow.
*       Otherwise return SW_SHOWDEFAULT.
*
*Entry:
*       None
*
*Exit:
*       The current show window mode.
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP WORD __cdecl __crtGetShowWindowMode(
        void
        )
{
    STARTUPINFOW startupInfo;

    GetStartupInfoW( &startupInfo );
    return startupInfo.dwFlags & STARTF_USESHOWWINDOW
                        ? startupInfo.wShowWindow
                        : SW_SHOWDEFAULT;
}

/*******************************************************************************
*__crtSetUnhandledExceptionFilter() - Sets the unhandled exception fitler.
*
*Purpose:
*       Used to set the unhandled exception filter.
*
*Entry:
*       A pointer to a top-level exception filter function that will be called
*       whenever the UnhandledExceptionFilter function gets control,
*
*Exit:
*       None.
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl __crtSetUnhandledExceptionFilter(
        LPTOP_LEVEL_EXCEPTION_FILTER exceptionFilter
        )
{
    /* Simply call the Win32 API. We don't need the previous exception filter,
       so the return value is ignored.
     */
    SetUnhandledExceptionFilter(exceptionFilter);
}

/*******************************************************************************
*__crtTerminateProcess() - Terminates the current process.
*
*Purpose:
*       Terminates the current process.
*       This function is not needed for ARM and Win8, which call __fastfail() instead.
*Entry:
*       The exit code to be used by the process and threads terminated
*
*Exit:
*       None
*
*Exceptions:
*
*******************************************************************************/

#if (defined (_M_IX86) || defined (_M_X64))
_CRTIMP void __cdecl __crtTerminateProcess (
        UINT uExitCode
        )
{
    /* Terminate the current process - the return code is currently unusable in
      the CRT, so we ignore it. */
    TerminateProcess(GetCurrentProcess(), uExitCode);
}
#endif  /* (defined (_M_IX86) || defined (_M_X64)) */

/*******************************************************************************
*__crtUnhandledException() - Raise an exception that will override all other exception filters.
*
*Purpose:
*       Raise an exception that will override all other exception filters.
*       In most cases, if the process is not being debugged, the function
*       displays an error message box (Watson box).
*
*       This function is not needed for ARM and Win8, which call __fastfail() instead.
*
*Entry:
*       pExceptionRecord: A pointer to an EXCEPTION_RECORD structure that contains
*       a machine-independent description of the exception
*
*Exit:
*       Returns EXCEPTION_CONTINUE_SEARCH or EXCEPTION_EXECUTE_HANDLER
*       if SEM_NOGPFAULTERRORBOX flag was specified in a previous call to SetErrorMode.
*
*Exceptions:
*
*******************************************************************************/
#if defined (_M_IX86) || defined (_M_X64)
_CRTIMP LONG __cdecl __crtUnhandledException (
        EXCEPTION_POINTERS *exceptionInfo
        )
{
    /* specifies default handling within UnhandledExceptionFilter */
    SetUnhandledExceptionFilter(NULL);

    /* invoke default exception filter */
    return UnhandledExceptionFilter(exceptionInfo);
}
#endif  /* defined (_M_IX86) || defined (_M_X64) */

/*******************************************************************************
*__crtCapturePreviousContext() - Capture the context record of the frame "preceding" the
      caller of this function.
*
*     Only available for x64.
*
*Exit:
*       pContextRecord have the context information of the frame preceding the
*       caller.
*
*Exceptions:
*
*******************************************************************************/
#if defined (_M_X64)
_CRTIMP void __cdecl __crtCapturePreviousContext (
        CONTEXT *pContextRecord
        )
{
    ULONG64 ControlPc;
    ULONG64 EstablisherFrame;
    ULONG64 ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    PVOID HandlerData;
    INT frames;

    RtlCaptureContext(pContextRecord);

    ControlPc = pContextRecord->Rip;

    /* Unwind "twice" to get the context of the caller to the "previous" caller. */
    for (frames = 0; frames < 2; ++frames)
    {
        FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, NULL);

        if (FunctionEntry != NULL) {
            RtlVirtualUnwind(0, /* UNW_FLAG_NHANDLER */
                             ImageBase,
                             ControlPc,
                             FunctionEntry,
                             pContextRecord,
                             &HandlerData,
                             &EstablisherFrame,
                             NULL);
        } else {
            break;
        }
    }
}
#endif  /* defined (_M_X64) */

/*******************************************************************************
*__crtCaptureCurrentContext() - Capture the context record of the caller of this function.
*
*     Only available for x64.
*
*Exit:
*       pContextRecord have the context information of the  caller.
*
*Exceptions:
*
*******************************************************************************/
#if defined (_M_X64)
_CRTIMP void __cdecl __crtCaptureCurrentContext (
        CONTEXT *pContextRecord
        )
{

    ULONG64 ControlPc;
    ULONG64 EstablisherFrame;
    ULONG64 ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    PVOID HandlerData;

    RtlCaptureContext(pContextRecord);

    ControlPc = pContextRecord->Rip;
    FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, NULL);

    if (FunctionEntry != NULL) {
        RtlVirtualUnwind(0, /* UNW_FLAG_NHANDLER */
                            ImageBase,
                            ControlPc,
                            FunctionEntry,
                            pContextRecord,
                            &HandlerData,
                            &EstablisherFrame,
                            NULL);
    }
}
#endif  /* defined (_M_X64) */

#endif  /* _CRT_APP */

#if _CRT_NTDDI_MIN < NTDDI_VISTA 

/*******************************************************************************
* Wrapper for Win32 GetTickCount functionality
*
******************************************************************************/

ULONGLONG __cdecl __crtGetTickCount64()
{
    // use GetTickCount64 if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNGETTICKCOUNT64, GetTickCount64, pfGetTickCount64)
    {
        return (*pfGetTickCount64)();
    }

    // ...otherwise fall back to using GetTickCount.
    return GetTickCount();
}

/*******************************************************************************
* Wrappers for Win32 thread/fiber-level storage APIs.
*
*******************************************************************************/

_CRTIMP DWORD __cdecl __crtFlsAlloc(
  PFLS_CALLBACK_FUNCTION lpCallback)
{
#if _CRT_NTDDI_MIN >= NTDDI_WS03 
    return FlsAlloc(lpCallback);
#else  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
    // use FlsAlloc if it is available (only on Server 2003+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFLSALLOC, FlsAlloc, pfFlsAlloc)
    {
        return (*pfFlsAlloc)(lpCallback);
    }

    // ...otherwise fall back to using TlsAlloc.
    return TlsAlloc();
#endif  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
}

_CRTIMP BOOL __cdecl __crtFlsFree(
  DWORD dwFlsIndex)
{
#if _CRT_NTDDI_MIN >= NTDDI_WS03 
    return FlsFree(dwFlsIndex);
#else  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
    // use FlsFree if it is available (only on Server 2003+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFLSFREE, FlsFree, pfFlsFree)
    {
        return (*pfFlsFree)(dwFlsIndex);
    }

    // ...otherwise fall back to using TlsFree.
    return TlsFree(dwFlsIndex);
#endif  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
}

_CRTIMP PVOID __cdecl __crtFlsGetValue(
  DWORD dwFlsIndex)
{
#if _CRT_NTDDI_MIN >= NTDDI_WS03 
    return FlsGetValue(dwFlsIndex);
#else  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
    // use FlsGetValue if it is available (only on Server 2003+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFLSGETVALUE, FlsGetValue, pfFlsGetValue)
    {
        return (*pfFlsGetValue)(dwFlsIndex);
    }

    // ...otherwise fall back to using TlsGetValue.
    return TlsGetValue(dwFlsIndex);
#endif  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
}

_CRTIMP BOOL __cdecl __crtFlsSetValue(
  DWORD dwFlsIndex,
  PVOID lpFlsData)
{
#if _CRT_NTDDI_MIN >= NTDDI_WS03 
    return FlsSetValue(dwFlsIndex, lpFlsData);
#else  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
    // use FlsSetValue if it is available (only on Server 2003+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFLSSETVALUE, FlsSetValue, pfFlsSetValue)
    {
        return (*pfFlsSetValue)(dwFlsIndex, lpFlsData);
    }

    // ...otherwise fall back to using TlsSetValue.
    return TlsSetValue(dwFlsIndex, lpFlsData);
#endif  /* _CRT_NTDDI_MIN >= NTDDI_WS03  */
}

/*******************************************************************************
* Wrappers for Win32 APIs.
*
*******************************************************************************/

_CRTIMP BOOL __cdecl __crtInitializeCriticalSectionEx(
  __out LPCRITICAL_SECTION lpCriticalSection,
  __in DWORD dwSpinCount,
  __in DWORD Flags)
{
    // use InitializeCriticalSectionEx if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNINITIALIZECRITICALSECTIONEX, InitializeCriticalSectionEx, pfInitializeCriticalSectionEx)
    {
        return (*pfInitializeCriticalSectionEx)(lpCriticalSection, dwSpinCount, Flags);
    }

    // ...otherwise fall back to using InitializeCriticalSectionAndSpinCount.
    InitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount);
    return TRUE;
}

_CRTIMP HANDLE __cdecl __crtCreateEventExW(
  _In_opt_  LPSECURITY_ATTRIBUTES lpEventAttributes,
  _In_opt_  LPCWSTR lpName,
  _In_      DWORD dwFlags,
  _In_      DWORD dwDesiredAccess)
{
    // use CreateEventEx if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCREATEEVENTEXW, CreateEventExW, pfCreateEventExW)
    {
        return (*pfCreateEventExW)(lpEventAttributes, lpName, dwFlags, dwDesiredAccess);
    }

    // ...otherwise fall back to using CreateEvent.
    return CreateEventW(lpEventAttributes, dwFlags & CREATE_EVENT_MANUAL_RESET, dwFlags & CREATE_EVENT_INITIAL_SET, lpName);
}

_CRTIMP HANDLE __cdecl __crtCreateSemaphoreExW(
  __in_opt    LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
  __in        LONG lInitialCount,
  __in        LONG lMaximumCount,
  __in_opt    LPCWSTR lpName,
  __reserved  DWORD dwFlags,
  __in        DWORD dwDesiredAccess)
{
    // use CreateSemaphoreEx if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCREATESEMAPHOREEXW, CreateSemaphoreExW, pfCreateSemaphoreExW)
    {
        return (*pfCreateSemaphoreExW)(lpSemaphoreAttributes, lInitialCount, lMaximumCount, lpName, dwFlags, dwDesiredAccess);
    }

    // ...otherwise fall back to using CreateSemaphore.
    return CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, lpName);
}

_CRTIMP BOOL __cdecl __crtSetThreadStackGuarantee(
  PULONG StackSizeInBytes)
{
    // use SetThreadStackGuarantee if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNSETTHREADSTACKGUARANTEE, SetThreadStackGuarantee, pfSetThreadStackGuarantee)
    {
        return (*pfSetThreadStackGuarantee)(StackSizeInBytes);
    }

    // ...otherwise there is no fall back.
    return FALSE;
}

PTP_TIMER __cdecl __crtCreateThreadpoolTimer(
  __in         PTP_TIMER_CALLBACK pfnti,
  __inout_opt  PVOID pv,
  __in_opt     PTP_CALLBACK_ENVIRON pcbe)
{
    // use CreateThreadpoolTimer if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCREATETHREADPOOLTIMER, CreateThreadpoolTimer, pfCreateThreadpoolTimer)
    {
        return (*pfCreateThreadpoolTimer)(pfnti, pv, pcbe);
    }

    // ...otherwise return failure because there is no fall back.
    return NULL;
}

VOID __cdecl __crtSetThreadpoolTimer(
  __inout   PTP_TIMER pti,
  __in_opt  PFILETIME pftDueTime,
  __in      DWORD msPeriod,
  __in_opt  DWORD msWindowLength)
{
    // use SetThreadpoolTimer if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNSETTHREADPOOLTIMER, SetThreadpoolTimer, pfSetThreadpoolTimer)
    {
        (*pfSetThreadpoolTimer)(pti, pftDueTime, msPeriod, msWindowLength);
    }

    // ...otherwise there is no fall back.
    return;
}

VOID __cdecl __crtWaitForThreadpoolTimerCallbacks(
  __inout  PTP_TIMER pti,
  __in     BOOL fCancelPendingCallbacks)
{
    // use WaitForThreadpoolTimerCallbacks if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNWAITFORTHREADPOOLTIMERCALLBACKS, WaitForThreadpoolTimerCallbacks, pfWaitForThreadpoolTimerCallbacks)
    {
        (*pfWaitForThreadpoolTimerCallbacks)(pti, fCancelPendingCallbacks);
    }

    // ...otherwise there is no fall back.
    return;
}

VOID __cdecl __crtCloseThreadpoolTimer(
  __inout  PTP_TIMER pti)
{
    // use CloseThreadpoolTimer if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCLOSETHREADPOOLTIMER, CloseThreadpoolTimer, pfCloseThreadpoolTimer)
    {
        (*pfCloseThreadpoolTimer)(pti);
    }

    // ...otherwise there is no fall back.
    return;
}

PTP_WAIT __cdecl __crtCreateThreadpoolWait(
  __in         PTP_WAIT_CALLBACK pfnwa,
  __inout_opt  PVOID pv,
  __in_opt     PTP_CALLBACK_ENVIRON pcbe)
{
    // use CreateThreadpoolWait if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCREATETHREADPOOLWAIT, CreateThreadpoolWait, pfCreateThreadpoolWait)
    {
        return (*pfCreateThreadpoolWait)(pfnwa, pv, pcbe);
    }

    // ...otherwise return failure because there is no fall back.
    return NULL;
}

VOID __cdecl __crtSetThreadpoolWait(
  __inout   PTP_WAIT pwa,
  __in_opt  HANDLE h,
  __in_opt  PFILETIME pftTimeout)
{
    // use SetThreadpoolWait if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNSETTHREADPOOLWAIT, SetThreadpoolWait, pfSetThreadpoolWait)
    {
        (*pfSetThreadpoolWait)(pwa, h, pftTimeout);
    }

    // ...otherwise there is no fall back.
    return;
}

VOID __cdecl __crtCloseThreadpoolWait(
  __inout  PTP_WAIT pwa)
{
    // use CloseThreadpoolWait if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCLOSETHREADPOOLWAIT, CloseThreadpoolWait, pfCloseThreadpoolWait)
    {
        (*pfCloseThreadpoolWait)(pwa);
    }

    // ...otherwise there is no fall back.
    return;
}

VOID __cdecl __crtFlushProcessWriteBuffers(void)
{
    // use FlushProcessWriteBuffers if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFLUSHPROCESSWRITEBUFFERS, FlushProcessWriteBuffers, pfFlushProcessWriteBuffers)
    {
        (*pfFlushProcessWriteBuffers)();
    }

    // ...otherwise there is no fall back.
    return;
}

VOID __cdecl __crtFreeLibraryWhenCallbackReturns(
  __inout  PTP_CALLBACK_INSTANCE pci,
  __in     HMODULE mod)
{
    // use FreeLibraryWhenCallbackReturns if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNFREELIBRARYWHENCALLBACKRETURNS, FreeLibraryWhenCallbackReturns, pfFreeLibraryWhenCallbackReturns)
    {
        (*pfFreeLibraryWhenCallbackReturns)(pci, mod);
    }

    // ...otherwise there is no fall back.
    return;
}

DWORD __cdecl __crtGetCurrentProcessorNumber(void)
{
    // use GetCurrentProcessorNumber if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNGETCURRENTPROCESSORNUMBER, GetCurrentProcessorNumber, pfGetCurrentProcessorNumber)
    {
        return (*pfGetCurrentProcessorNumber)();
    }

    // ...otherwise return 0 because there is no fall back.
    return 0;
}

BOOL __cdecl __crtGetLogicalProcessorInformation(
  __out    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer,
  __inout  PDWORD ReturnLength)
{
    // use GetLogicalProcessorInformation if it is available (only on Windows XP SP3+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNGETLOGICALPROCESSORINFORMATION, GetLogicalProcessorInformation, pfGetLogicalProcessorInformation)
    {
        return (*pfGetLogicalProcessorInformation)(Buffer, ReturnLength);
    }

    // ...otherwise return 0 and set error code because there is no fall back.
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

_CRTIMP BOOLEAN __cdecl __crtCreateSymbolicLinkW(
  __in  LPCWSTR lpSymlinkFileName,
  __in  LPCWSTR lpTargetFileName,
  __in  DWORD dwFlags)
{
    // use CreateSymbolicLink if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNCREATESYMBOLICLINK, CreateSymbolicLinkW, pfCreateSymbolicLink)
    {
        return (*pfCreateSymbolicLink)(lpSymlinkFileName, lpTargetFileName, dwFlags);
    }

    // ...otherwise return 0 and set error code because there is no fall back.
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

_CRTIMP BOOL __cdecl __crtGetFileInformationByHandleEx(
  _In_   HANDLE hFile,
  _In_   FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
  _Out_  LPVOID lpFileInformation,
  _In_   DWORD dwBufferSize)
{
    // use GetFileInformationByHandleEx if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNGETFILEINFORMATIONBYHANDLEEX, GetFileInformationByHandleExW, pfGetFileInformationByHandleEx)
    {
        return (*pfGetFileInformationByHandleEx)(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
    }

    // ...otherwise return 0 and set error code because there is no fall back.
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

_CRTIMP BOOL __cdecl __crtSetFileInformationByHandle(
  _In_  HANDLE hFile,
  _In_  FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
  _In_  LPVOID lpFileInformation,
  _In_  DWORD dwBufferSize)
{
    // use SetFileInformationByHandle if it is available (only on Vista+)...
    IFDYNAMICGETCACHEDFUNCTION(KERNEL32, PFNSETFILEINFORMATIONBYHANDLE, SetFileInformationByHandleW, pfSetFileInformationByHandle)
    {
        return (*pfSetFileInformationByHandle)(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
    }

    // ...otherwise return 0 and set error code because there is no fall back.
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

#endif  /* _CRT_NTDDI_MIN < NTDDI_VISTA  */

/*******************************************************************************
* Wrapper for Win32 Sleep() API.
*
*******************************************************************************/
_CRTIMP void __cdecl __crtSleep(_In_ DWORD dwMilliseconds)
{
#if defined (_CRT_APP) && !defined (_KERNELX)
    HANDLE handle = CreateEventExW(NULL, NULL, 0, EVENT_ALL_ACCESS );
    if (handle)
    {
        /* Unlike Sleep/SleepEx, WaitForSingleObjectEx will not yield when the input
         * is 0 millis,  so we use the smallest possible value, 1, instead.
         */
        WaitForSingleObjectEx(handle, dwMilliseconds == 0 ? 1 : dwMilliseconds, FALSE);
        CloseHandle(handle);
    }
#else  /* defined (_CRT_APP) && !defined (_KERNELX) */
    Sleep(dwMilliseconds);
#endif  /* defined (_CRT_APP) && !defined (_KERNELX) */
}

/////////////////////////////////////////////////////////////////////////////
// Helper to load all necessary Win32 API function pointers

PVOID encodedKERNEL32Functions[eMaxKernel32Function];

void __cdecl __crtLoadWinApiPointers(void)
{
    HINSTANCE hKernel32 = GetModuleHandleW(L"kernel32.dll");

    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FlsAlloc);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FlsFree);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FlsGetValue);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FlsSetValue);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, InitializeCriticalSectionEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CreateEventExW);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CreateSemaphoreExW);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, SetThreadStackGuarantee);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CreateThreadpoolTimer);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, SetThreadpoolTimer);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, WaitForThreadpoolTimerCallbacks);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CloseThreadpoolTimer);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CreateThreadpoolWait);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, SetThreadpoolWait);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CloseThreadpoolWait);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FlushProcessWriteBuffers);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, FreeLibraryWhenCallbackReturns);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetCurrentProcessorNumber);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetLogicalProcessorInformation);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CreateSymbolicLinkW);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, SetDefaultDllDirectories);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, EnumSystemLocalesEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, CompareStringEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetDateFormatEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetLocaleInfoEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetTimeFormatEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetUserDefaultLocaleName);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, IsValidLocaleName);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, LCMapStringEx);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetCurrentPackageId);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetTickCount64);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, GetFileInformationByHandleExW);
    STOREENCODEDFUNCTIONPOINTER(hKernel32, KERNEL32, SetFileInformationByHandleW);
}

