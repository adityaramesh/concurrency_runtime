/***
*dllcrt0.c - C runtime initialization routine for a DLL with linked-in C R-T
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This the startup routine for a DLL which is linked with its own
*       C run-time code.  It is similar to the routine _mainCRTStartup()
*       in the file CRT0.C, except that there is no main() in a DLL.
*
*******************************************************************************/

#ifndef CRTDLL

#include <cruntime.h>
#include <dos.h>
#include <internal.h>
#include <mtdll.h>
#include <stdlib.h>
#include <string.h>
#include <rterr.h>
#include <oscalls.h>
#define _DECL_DLLMAIN   /* enable prototypes for DllMain and _CRT_INIT */
#include <process.h>
#include <awint.h>
#include <tchar.h>
#include <dbgint.h>
#include <rtcapi.h>
#include <locale.h>

/*
 * flag set iff _CRTDLL_INIT was called with DLL_PROCESS_ATTACH
 */
static int __proc_attached = 0;

/*
 * command line, environment, and a few other globals
 */
char *_acmdln;              /* points to command line */

char *_aenvptr = NULL;      /* points to environment block */
wchar_t *_wenvptr = NULL;   /* points to wide environment block */

/*
 * User routine DllMain is called on all notifications
 */
extern BOOL WINAPI DllMain(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        ) ;

/*
 * _pRawDllMain MUST be an extern const variable, which will be aliased to
 * _pDefaultRawDllMain if no real user definition is present, thanks to the
 * alternatename directive.
 */

extern BOOL (WINAPI * const _pRawDllMain)(HANDLE, DWORD, LPVOID);
extern BOOL (WINAPI * const _pDefaultRawDllMain)(HANDLE, DWORD, LPVOID) = NULL;
#if defined (_M_IX86)
#pragma comment(linker, "/alternatename:__pRawDllMain=__pDefaultRawDllMain")
#elif defined (_M_X64) || defined (_M_ARM)
#pragma comment(linker, "/alternatename:_pRawDllMain=_pDefaultRawDllMain")
#else  /* defined (_M_X64) || defined (_M_ARM) */
#error Unsupported platform
#endif  /* defined (_M_X64) || defined (_M_ARM) */

/***
*BOOL WINAPI _CRT_INIT(hDllHandle, dwReason, lpreserved) -
*       C Run-Time initialization for a DLL linked with a C run-time library.
*
*Purpose:
*       This routine does the C run-time initialization or termination.
*       For the multi-threaded run-time library, it also cleans up the
*       multi-threading locks on DLL termination.
*
*Entry:
*
*Exit:
*
*NOTES:
*       This routine must be the entry point for the DLL.
*
*******************************************************************************/

BOOL WINAPI _CRT_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
        /*
         * Start-up code only gets executed when the process is initialized
         */

        if ( dwReason == DLL_PROCESS_ATTACH )
        {
            if ( !_heap_init() )    /* initialize heap */
                return FALSE;       /* fail to load DLL */

            if(!_mtinit())          /* initialize multi-thread */
            {
                _heap_term();       /* heap is now invalid! */
                return FALSE;       /* fail to load DLL */
            }

            /*
             * Initialize the Runtime Checks stuff
             */
#ifdef _RTC
            _RTC_Initialize();
#endif  /* _RTC */
            _acmdln = (char *)GetCommandLineA();
            _aenvptr = (char *)__crtGetEnvironmentStringsA();

            if (_ioinit() < 0)
            {
                _mtterm();          /* free TLS index, call _mtdeletelocks() */
                _heap_term();       /* heap is now invalid! */
                return FALSE;       /* fail to load DLL */
            }


            if (_setargv() < 0 ||   /* get cmd line info */
                _setenvp() < 0 ||   /* get environ info */
                _cinit(FALSE) != 0) /* do C data initialize, but don't init floating point */
            {
                _ioterm();          /* shut down lowio */
                _mtterm();          /* free TLS index, call _mtdeletelocks() */
                _heap_term();       /* heap is now invalid! */
                return FALSE;       /* fail to load DLL */
            }

            /* Enable buffer count checking if linking against static lib */
            _CrtSetCheckCount(TRUE);

            /*
             * increment flag to indicate process attach notification
             * has been received
             */
            __proc_attached++;
        }

        else if ( dwReason == DLL_PROCESS_DETACH )
        {
            if ( __proc_attached > 0 )
            {
                __proc_attached--;
                __try
                {
                    /*
                     * Any basic clean-up code that goes here must be duplicated
                     * below in _DllMainCRTStartup for the case where the user's
                     * DllMain() routine fails on a Process Attach notification.
                     * This does not include calling user C++ destructors, etc.
                     */

                    if ( _C_Termination_Done == FALSE )
                        _cexit();

                    /*
                     * What remains is to clean up the system resources we have
                     * used (handles, critical sections, memory,...,etc.). This
                     * needs to be done if the whole process is NOT terminating.
                     */

                    /* Free allocated CRT memory */
                    __freeCrtMemory();

#ifdef _DEBUG
                    /* Dump all memory leaks */
                    if ( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & _CRTDBG_LEAK_CHECK_DF )
                    {
                        _CrtSetDumpClient(NULL);
                        _CrtDumpMemoryLeaks();
                    }
#endif  /* _DEBUG */

#ifndef _DEBUG
                    /* If dwReason is DLL_PROCESS_DETACH, lpreserved is NULL
                     * if FreeLibrary has been called or the DLL load failed
                     * and non-NULL if the process is terminating.
                     */
                    if ( lpreserved == NULL )
                    {
#endif  /* _DEBUG */
                        /*
                         * The process is NOT terminating so we must clean up...
                         */
                        /* Shut down lowio */
                        _ioterm();
                        _mtterm();

                        /* This should be the last thing the C run-time does */
                        _heap_term();   /* heap is now invalid! */
#ifndef _DEBUG
                    }
#endif  /* _DEBUG */
                }
                __finally {
                    /* we shouldn't really have to care about this, because
                       letting an exception escape from DllMain(DLL_PROCESS_DETACH) should
                       result in process termination. Unfortunately, Windows up to Win7 as of now
                       just silently swallows the exception.

                       I have considered all kinds of tricks, but decided to leave it up to OS
                       folks to fix this.

                       For the time being just remove our FLS callback during phase 2 unwind
                       that would otherwise be left pointing to unmapped address space.
                     */
                     if ( lpreserved == NULL && __flsindex != FLS_OUT_OF_INDEXES )
                          _mtterm();
                 }
            }
            else
                /* no prior process attach, just return */
                    return FALSE;

        }
        else if ( dwReason == DLL_THREAD_ATTACH )
        {
            _ptiddata ptd;

            /* Initialize per-thread data struct after verifying
             * ptd has not been allocated already.
             */
            if ( (ptd = __crtFlsGetValue(__flsindex)) == NULL )
            {
                if ( ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) != NULL))
                {
                    if (__crtFlsSetValue(__flsindex, (LPVOID)ptd) ) {
                        /*
                         * Initialize of per-thread data
                         */
                        _initptd(ptd, NULL);

                        ptd->_tid = GetCurrentThreadId();
                        ptd->_thandle = (uintptr_t)(-1);
                    }
                    else
                    {
                        _free_crt(ptd);
                        return FALSE;
                    }
                }
                else
                {
                    return FALSE;
                }
            }
        }
        else if ( dwReason == DLL_THREAD_DETACH )
        {
            _freeptd(NULL);         /* free up per-thread CRT data */
        }

        return TRUE ;
}

/***
*int __DllXcptFilter(hDllHandle, dwReason, lpreserved, xcptnum, pxcptptrs, dwReason) -
*       Wrapper over __CppXcptFilter that also ensures the dll is
*       cleaned up properly if an exception occurs during initializing the dll.
*
*******************************************************************************/

int __cdecl __DllXcptFilter (
    HANDLE  hDllHandle,
    DWORD   dwReason,
    LPVOID  lpreserved,
    unsigned long xcptnum,
    PEXCEPTION_POINTERS pxcptinfoptrs
    )
{
    if ( dwReason == DLL_PROCESS_ATTACH )
    {
        /*
            * The user's DllMain routine failed with an unhandled exception, so the C runtime
            * needs to be cleaned up. Do this by calling _CRT_INIT again, this time imitating
            * DLL_PROCESS_DETACH.
            * Note, we don't call the user's DllMain again since the DLL hasn't loaded, and
            * is still in unknown state due to the unhandled exception.
            */
        _CRT_INIT(hDllHandle, DLL_PROCESS_DETACH, lpreserved);
    }

    return __CppXcptFilter(xcptnum, pxcptinfoptrs);
}


/***
*BOOL WINAPI _DllMainCRTStartup(hDllHandle, dwReason, lpreserved) -
*       C Run-Time initialization for a DLL linked with a C run-time library.
*
*Purpose:
*       This routine does the C run-time initialization or termination
*       and then calls the user code notification handler "DllMain".
*       For the multi-threaded run-time library, it also cleans up the
*       multi-threading locks on DLL termination.
*
*Entry:
*
*Exit:
*
*NOTES:
*       This routine is the preferred entry point. _CRT_INIT may also be
*       used, or the user may supply his/her own entry and call _CRT_INIT
*       from within it, but this is not the preferred method.
*
*******************************************************************************/

static
BOOL __cdecl
__DllMainCRTStartup(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        );

BOOL WINAPI
_DllMainCRTStartup(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
        if (dwReason == DLL_PROCESS_ATTACH)
        {
        /*
         * The /GS security cookie must be initialized before any exception
         * handling targetting the current image is registered.  No function
         * using exception handling can be called in the current image until
         * after __security_init_cookie has been called.
         */
            __security_init_cookie();
        }

        return __DllMainCRTStartup(hDllHandle, dwReason, lpreserved);
}

__declspec(noinline)
BOOL __cdecl
__DllMainCRTStartup(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
        BOOL retcode = TRUE;

        /*
         * If this is a process detach notification, check that there has
         * has been a prior process attach notification.
         */
        if ( (dwReason == DLL_PROCESS_DETACH) && (__proc_attached == 0) )
            /*
             * no prior process attach notification. just return
             * without doing anything.
             */
            return FALSE;

        __try {
            if ( dwReason == DLL_PROCESS_ATTACH || dwReason == DLL_THREAD_ATTACH )
            {
                if ( _pRawDllMain )
                    retcode = (*_pRawDllMain)(hDllHandle, dwReason, lpreserved);

                if ( retcode )
                    retcode = _CRT_INIT(hDllHandle, dwReason, lpreserved);

                if ( !retcode )
                    return FALSE;
            }

            retcode = DllMain(hDllHandle, dwReason, lpreserved);

            if ( (dwReason == DLL_PROCESS_ATTACH) && !retcode )
            {
                /*
                 * The user's DllMain routine returned failure, the C runtime
                 * needs to be cleaned up. Do this by calling _CRT_INIT again,
                 * this time imitating DLL_PROCESS_DETACH. Note this will also
                 * clear the __proc_attached flag so the cleanup will not be
                 * repeated upon receiving the real process detach notification.
                 */
                DllMain(hDllHandle, DLL_PROCESS_DETACH, lpreserved);
                _CRT_INIT(hDllHandle, DLL_PROCESS_DETACH, lpreserved);
                if (_pRawDllMain)
                {
                    (*_pRawDllMain)(hDllHandle, DLL_PROCESS_DETACH, lpreserved);
                }
            }

            if ( (dwReason == DLL_PROCESS_DETACH) ||
                 (dwReason == DLL_THREAD_DETACH) )
            {
                if ( _CRT_INIT(hDllHandle, dwReason, lpreserved) == FALSE )
                    retcode = FALSE ;

                if ( retcode && _pRawDllMain )
                    retcode = (*_pRawDllMain)(hDllHandle, dwReason, lpreserved);
            }
        } __except ( __DllXcptFilter(hDllHandle, dwReason, lpreserved,
                                     GetExceptionCode(), GetExceptionInformation()) ) {
            return FALSE;
        }

        return retcode ;
}

#endif  /* CRTDLL */
