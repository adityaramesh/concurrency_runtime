/***
*threadex.c - Extended versions of Begin (Create) and End (Exit) a Thread
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This source contains the _beginthreadex() and _endthreadex()
*       routines which are used to start and terminate a thread.  These
*       routines are more like the Win32 APIs CreateThread() and ExitThread()
*       than the original functions _beginthread() & _endthread() were.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <malloc.h>
#include <process.h>
#include <stddef.h>
#include <rterr.h>
#include <dbgint.h>
#include <errno.h>
#include <awint.h>
#ifdef MRTDLL
#include <msclr\appdomain.h>
#endif  /* MRTDLL */

#pragma warning(disable:4439)   // C4439: function with a managed parameter must have a __clrcall calling convention

#if defined(_CRT_APP) && !defined(_KERNELX)
#error This source file is not available for _CRT_APP
#endif  /* _CRT_APP */

/*
 * Startup code for new thread.
 */
static unsigned long WINAPI _threadstartex(void *);
static void _callthreadstartex(void);

#ifndef __CLR_OR_STD_CALL
#if defined (_M_CEE) || defined (MRTDLL)
#define __CLR_OR_STD_CALL   __clrcall
#else  /* defined (_M_CEE) || defined (MRTDLL) */
#define __CLR_OR_STD_CALL   __stdcall
#endif  /* defined (_M_CEE) || defined (MRTDLL) */
#endif  /* __CLR_OR_STD_CALL */

#ifdef MRTDLL
static int _getdomain(DWORD *pDomain)
{
    using System::Runtime::InteropServices::RuntimeEnvironment;

    *pDomain=0;

    // Throws HR exception on failure.
    ICLRRuntimeHost *pClrHost = NULL;
    pClrHost = reinterpret_cast<ICLRRuntimeHost*>(
        RuntimeEnvironment::GetRuntimeInterfaceAsIntPtr(
            msclr::_detail::FromGUID(CLSID_CLRRuntimeHost),
            msclr::_detail::FromGUID(IID_ICLRRuntimeHost)).ToPointer());

    DWORD domain=0;
    HRESULT hr = pClrHost->GetCurrentAppDomainId(&domain);
    pClrHost->Release();
    pClrHost=NULL;
    if (FAILED(hr))
    {
        return false;
    }
    *pDomain=domain;
    return true;
}
#endif  /* MRTDLL */

/* Ro initialization flags; passed to Windows::Runtime::Initialize */
typedef enum RO_INIT_TYPE
{
    RO_INIT_SINGLETHREADED     = 0,      /* Single-threaded application */
    RO_INIT_MULTITHREADED      = 1,      /* COM calls objects on any thread. */
} RO_INIT_TYPE;

static int _initMTAoncurrentthread()
{
    static void * volatile s_roinit;
    static volatile int s_initialized;

    if (!s_initialized)
    {
        void *pfn = GetProcAddress(LoadLibraryExW(L"combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), "RoInitialize");
        if (pfn == NULL)
            return 0;

        s_roinit = EncodePointer(pfn);
        s_initialized = 1;
    }
    return ((HRESULT (WINAPI *) (RO_INIT_TYPE)) DecodePointer(s_roinit)) (RO_INIT_MULTITHREADED) == S_OK;
}

static void _uninitMTAoncurrentthread()
{
    static void * volatile s_rouninit;
    static volatile int s_initialized;

    if (!s_initialized)
    {
        void *pfn = GetProcAddress(LoadLibraryExW(L"combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), "RoUninitialize");
        if (pfn == NULL)
            return;
        s_rouninit = EncodePointer(pfn);
        s_initialized = 1;
    }
    ((void (WINAPI *)()) DecodePointer(s_rouninit))();
}

/*
 * CreateThread wrapper
 */
static HANDLE _createThread(
        LPSECURITY_ATTRIBUTES security,
        unsigned stacksize,
        LPVOID ptd,
        unsigned createflag,
        LPDWORD thrdaddr)
{
    return CreateThread(
                security,
                stacksize,
                _threadstartex,
                ptd,
                createflag,
                thrdaddr);
}

/***
*_beginthreadex() - Create a child thread
*
*Purpose:
*       Create a child thread.
*
*Entry:
*       *** Same parameters as the Win32 API CreateThread() ***
*       security = security descriptor for the new thread
*       stacksize = size of stack
*       initialcode = pointer to thread's startup code address
*               must be a __stdcall function returning an unsigned.
*       argument = argument to be passed to new thread
*       createflag = flag to create thread in a suspended state
*       thrdaddr = points to an int to receive the ID of the new thread
*
*Exit:
*       *** Same as the Win32 API CreateThread() ***
*
*       success = handle for new thread if successful
*
*       failure = 0 in case of error, errno and _doserrno are set
*
*Exceptions:
*
*Notes:
*       This routine is more like the Win32 API CreateThread() than it
*       is like the C run-time routine _beginthread().  Ditto for
*       _endthreadex() and the Win32 API ExitThread() versus _endthread().
*
*       Differences between _beginthread/_endthread and the "ex" versions:
*
*         1)  _beginthreadex takes the 3 extra parameters to CreateThread
*             which are lacking in _beginthread():
*               A) security descriptor for the new thread
*               B) initial thread state (running/asleep)
*               C) pointer to return ID of newly created thread
*
*         2)  The routine passed to _beginthread() must be __cdecl and has
*             no return code, but the routine passed to _beginthreadex()
*             must be __stdcall and returns a thread exit code.  _endthread
*             likewise takes no parameter and calls ExitThread() with a
*             parameter of zero, but _endthreadex() takes a parameter as
*             thread exit code.
*
*         3)  _endthread implicitly closes the handle to the thread, but
*             _endthreadex does not!
*
*         4)  _beginthread returns -1 for failure, _beginthreadex returns
*             0 for failure (just like CreateThread).
*
*******************************************************************************/

_CRTIMP uintptr_t __cdecl _beginthreadex (
        void *security,
        unsigned stacksize,
        unsigned (__stdcall * initialcode) (void *),
        void * argument,
        unsigned createflag,
        unsigned *thrdaddr
        )
{
        _ptiddata ptd;               /* pointer to per-thread data */
        uintptr_t thdl;              /* thread handle */
        unsigned long err = 0L;      /* Return from GetLastError() */
        unsigned dummyid;            /* dummy returned thread ID */

        /* validation section */
        _VALIDATE_RETURN(initialcode != NULL, EINVAL, 0);

        /*
         * Allocate and initialize a per-thread data structure for the to-
         * be-created thread.
         */
        if ( (ptd = (_ptiddata)_calloc_crt(1, sizeof(struct _tiddata))) == NULL )
                goto error_return;

        /*
         * Initialize the per-thread data
         */

        _initptd(ptd, _getptd()->ptlocinfo);

        ptd->_initaddr = (void *) initialcode;
        ptd->_initarg = argument;
        ptd->_thandle = (uintptr_t)(-1);

#if defined (_M_CEE) || defined (MRTDLL)
        if(!_getdomain(&(ptd->__initDomain)))
        {
            goto error_return;
        }
#endif  /* defined (_M_CEE) || defined (MRTDLL) */

        /*
         * Make sure non-NULL thrdaddr is passed to CreateThread
         */
        if ( thrdaddr == NULL )
                thrdaddr = &dummyid;

        /*
         * Create the new thread using the parameters supplied by the caller.
         */
        if ( (thdl = (uintptr_t)
              _createThread( (LPSECURITY_ATTRIBUTES)security,
                            stacksize,
                            (LPVOID)ptd,
                            createflag,
                            (LPDWORD)thrdaddr))
             == (uintptr_t)0 )
        {
                err = GetLastError();
                goto error_return;
        }

        /*
         * Good return
         */
        return(thdl);

        /*
         * Error return
         */
error_return:
        /*
         * Either ptd is NULL, or it points to the no-longer-necessary block
         * calloc-ed for the _tiddata struct which should now be freed up.
         */
        _free_crt(ptd);

        /*
         * Map the error, if necessary.
         *
         * Note: this routine returns 0 for failure, just like the Win32
         * API CreateThread, but _beginthread() returns -1 for failure.
         */
        if ( err != 0L )
                _dosmaperr(err);

        return( (uintptr_t)0 );
}


/***
*_threadstartex() - New thread begins here
*
*Purpose:
*       The new thread begins execution here.  This routine, in turn,
*       passes control to the user's code.
*
*Entry:
*       void *ptd       = pointer to _tiddata structure for this thread
*
*Exit:
*       Never returns - terminates thread!
*
*Exceptions:
*
*******************************************************************************/

static unsigned long WINAPI _threadstartex (
        void * ptd
        )
{
        _ptiddata _ptd;                  /* pointer to per-thread data */

        /*
         * Check if ptd is initialised during THREAD_ATTACH call to dll mains
         */
        if ( ( _ptd = (_ptiddata)__crtFlsGetValue(__get_flsindex())) == NULL)
        {
            /*
             * Stash the pointer to the per-thread data stucture in TLS
             */
            if ( !__crtFlsSetValue(__get_flsindex(), ptd) )
                ExitThread(GetLastError());
            /*
             * Set the thread ID field -- parent thread cannot set it after
             * CreateThread() returns since the child thread might have run
             * to completion and already freed its per-thread data block!
             */
            ((_ptiddata) ptd)->_tid = GetCurrentThreadId();
            _ptd = ptd;
        }
        else
        {
            _ptd->_initaddr = ((_ptiddata) ptd)->_initaddr;
            _ptd->_initarg =  ((_ptiddata) ptd)->_initarg;
            _ptd->_thandle =  ((_ptiddata) ptd)->_thandle;
#if defined (_M_CEE) || defined (MRTDLL)
            _ptd->__initDomain=((_ptiddata) ptd)->__initDomain;
#endif  /* defined (_M_CEE) || defined (MRTDLL) */
            _freefls(ptd);
            ptd = _ptd;
        }


#if defined (_M_CEE) || defined (MRTDLL)
        DWORD domain=0;
        if(!_getdomain(&domain))
        {
            ExitThread(0);
        }
        if(domain!=_ptd->__initDomain)
        {
            /* need to transition to caller's domain and startup there*/
            ::msclr::call_in_appdomain(_ptd->__initDomain, _callthreadstartex);

            return 0L;
        }
#endif  /* defined (_M_CEE) || defined (MRTDLL) */

        _ptd->_initapartment = __crtIsPackagedApp();
        if (_ptd->_initapartment)
        {
            _ptd->_initapartment = _initMTAoncurrentthread();
        }

        _callthreadstartex();

        /*
         * Never executed!
         */
        return(0L);
}

static void _callthreadstartex(void)
{
    _ptiddata ptd;           /* pointer to thread's _tiddata struct */

    /* must always exist at this point */
    ptd = _getptd();

    /*
        * Guard call to user code with a _try - _except statement to
        * implement runtime errors and signal support
        */
    __try {
            _endthreadex (
                ( (unsigned (__CLR_OR_STD_CALL *)(void *))(((_ptiddata)ptd)->_initaddr) )
                ( ((_ptiddata)ptd)->_initarg ) ) ;
    }
    __except ( _XcptFilter(GetExceptionCode(), GetExceptionInformation()) )
    {
            /*
                * Should never reach here
                */
            _exit( GetExceptionCode() );

    } /* end of _try - _except */

}


#ifndef MRTDLL

/***
*_endthreadex() - Terminate the calling thread
*
*Purpose:
*
*Entry:
*       Thread exit code
*
*Exit:
*       Never returns!
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _endthreadex (
        unsigned retcode
        )
{
        _ptiddata ptd;           /* pointer to thread's _tiddata struct */
         HANDLE handle = NULL;

        ptd = _getptd_noexit();

        if (ptd) {
            if (ptd->_initapartment)
                _uninitMTAoncurrentthread();

            /*
             * Free up the _tiddata structure & its subordinate buffers
             *      _freeptd() will also clear the value for this thread
             *      of the FLS variable __flsindex.
             */
            _freeptd(ptd);
        }

        /*
         * Terminate the thread
         */
        ExitThread(retcode);

}
#endif  /* MRTDLL */
