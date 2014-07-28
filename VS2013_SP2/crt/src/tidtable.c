/***
*tidtable.c - Access thread data table
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module contains the following routines for multi-thread
*       data support:
*
*       _mtinit     = Initialize the mthread data
*       _getptd     = get the pointer to the per-thread data structure for
*                       the current thread
*       _freeptd    = free up a per-thread data structure and its
*                       subordinate structures
*       __threadid  = return thread ID for the current thread
*       __threadhandle = return pseudo-handle for the current thread
*
*******************************************************************************/

#include <sect_attribs.h>
#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <mtdll.h>
#include <memory.h>
#include <msdos.h>
#include <rterr.h>
#include <stdlib.h>
#include <stddef.h>
#include <dbgint.h>
#include <setlocal.h>
#include <mbstring.h>
#include <awint.h>

extern pthreadmbcinfo __ptmbcinfo;

extern threadlocinfo __initiallocinfo;
extern threadmbcinfo __initialmbcinfo;
extern pthreadlocinfo __ptlocinfo;

void * __cdecl __removelocaleref( pthreadlocinfo);
void __cdecl __addlocaleref( pthreadlocinfo);
void __cdecl __freetlocinfo(pthreadlocinfo);

unsigned long __flsindex = FLS_OUT_OF_INDEXES;


/***
* __get_flsindex - crt wrapper around __flsindex
*
* Purpose:
*       This function helps msvcmXX.dll beginthread and beginthreadex APIs
*       to retrive __flsindex value.
*
*******************************************************************************/

_CRTIMP unsigned long __cdecl __get_flsindex()
{
    return __flsindex;
}

/****
*_mtinit() - Init multi-thread data bases
*
*Purpose:
*       (1) Call _mtinitlocks to create/open all lock semaphores.
*       (2) Allocate a TLS index to hold pointers to per-thread data
*           structure.
*
*       NOTES:
*       (1) Only to be called ONCE at startup
*       (2) Must be called BEFORE any mthread requests are made
*
*Entry:
*       <NONE>
*Exit:
*       returns FALSE on failure
*
*Uses:
*       <any registers may be modified at init time>
*
*Exceptions:
*
*******************************************************************************/
int __cdecl _mtinit (
        void
        )
{
        _ptiddata ptd;

        _init_pointers();       /* initialize global function pointers */

        /*
         * Initialize the mthread lock data base
         */

        if ( !_mtinitlocks() ) {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

        /*
         * Allocate a TLS index to maintain pointers to per-thread data
         */
        if ( (__flsindex = __crtFlsAlloc(&_freefls)) == FLS_OUT_OF_INDEXES ) {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

        /*
         * Create a per-thread data structure for this (i.e., the startup)
         * thread.
         */
        if ( ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) == NULL) ||
             !__crtFlsSetValue(__flsindex, (LPVOID)ptd) )
        {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

        /*
         * Initialize the per-thread data
         */

        _initptd(ptd,NULL);

        ptd->_tid = GetCurrentThreadId();
        ptd->_thandle = (uintptr_t)(-1);

        return TRUE;
}


/****
*_mtterm() - Clean-up multi-thread data bases
*
*Purpose:
*       (1) Call _mtdeletelocks to free up all lock semaphores.
*       (2) Free up the TLS index used to hold pointers to
*           per-thread data structure.
*
*       NOTES:
*       (1) Only to be called ONCE at termination
*       (2) Must be called AFTER all mthread requests are made
*
*Entry:
*       <NONE>
*Exit:
*       returns
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _mtterm (
        void
        )
{
    /*
     * Free up the TLS index
     *
     * (Set __flsindex back to initial state (-1L).)
     */

    if ( __flsindex != FLS_OUT_OF_INDEXES ) {
        __crtFlsFree(__flsindex);
        __flsindex = FLS_OUT_OF_INDEXES;
    }

    /*
     * Clean up the mthread lock data base
     */

    _mtdeletelocks();
}



/***
*void _initptd(_ptiddata ptd, pthreadlocinfo) - initialize a per-thread data structure
*
*Purpose:
*       This routine handles all of the per-thread initialization
*       which is common to _beginthread, _beginthreadex, _mtinit
*       and _getptd.
*
*Entry:
*       pointer to a per-thread data block
*
*Exit:
*       the common fields in that block are initialized
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl _initptd (
        _ptiddata ptd,
        pthreadlocinfo ptloci
        )
{
    ptd->_pxcptacttab = (void *)_XcptActTab;
    ptd->_terrno = 0;
    ptd->_holdrand = 1L;

    // It is necessary to always have GLOBAL_LOCALE_BIT set in perthread data
    // because when doing bitwise or, we won't get __UPDATE_LOCALE to work when
    // global per thread locale is set.
    ptd->_ownlocale = _GLOBAL_LOCALE_BIT;

    // Initialize _setloc_data. These are the only valuse that need to be
    // initialized.
    ptd->_setloc_data._cachein[0]=L'C';
    ptd->_setloc_data._cacheout[0]=L'C';
    ptd->ptmbcinfo = &__initialmbcinfo;
    // Downlevel data is not initially used
    ptd->_setloc_downlevel_data = NULL;

    _mlock(_MB_CP_LOCK);
    __try
    {
        InterlockedIncrement(&(ptd->ptmbcinfo->refcount));
    }
    __finally
    {
        _munlock(_MB_CP_LOCK);
    }

        // We need to make sure that ptd->ptlocinfo in never NULL, this saves us
    // perf counts when UPDATING locale.
    _mlock(_SETLOCALE_LOCK);
    __try {
        ptd->ptlocinfo = ptloci;
        /*
         * Note that and caller to _initptd could have passed __ptlocinfo, but
         * that will be a bug as between the call to _initptd and __addlocaleref
         * the global locale may have changed and ptloci may be pointing to invalid
         * memory. Thus if the wants to set the locale to global, NULL should
         * be passed.
         */
        if (ptd->ptlocinfo == NULL)
            ptd->ptlocinfo = __ptlocinfo;
        __addlocaleref(ptd->ptlocinfo);
    }
    __finally {
        _munlock(_SETLOCALE_LOCK);
    }
}

/***
*_ptiddata _getptd_noexit(void) - get per-thread data structure for the current thread
*
*Purpose:
*
*Entry:
*
*Exit:
*       success = pointer to _tiddata structure for the thread
*       failure = NULL
*
*Exceptions:
*
*******************************************************************************/

_ptiddata __cdecl _getptd_noexit (
        void
        )
{
    _ptiddata ptd;
    DWORD   TL_LastError;

    TL_LastError = GetLastError();


    if ( (ptd = __crtFlsGetValue(__flsindex)) == NULL ) {
        /*
         * no per-thread data structure for this thread. try to create
         * one.
         */
#ifdef _DEBUG
        extern void * __cdecl _calloc_dbg_impl(size_t, size_t, int, const char *, int, int *);
        if ((ptd = _calloc_dbg_impl(1, sizeof(struct _tiddata), _CRT_BLOCK, __FILE__, __LINE__, NULL)) != NULL) {
#else  /* _DEBUG */
        if ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) != NULL) {
#endif  /* _DEBUG */

            if (__crtFlsSetValue(__flsindex, (LPVOID)ptd) ) {

                /*
                 * Initialize of per-thread data
                 */

                _initptd(ptd,NULL);

                ptd->_tid = GetCurrentThreadId();
                ptd->_thandle = (uintptr_t)(-1);
            }
            else {

                /*
                 * Return NULL to indicate failure
                 */

                _free_crt(ptd);
                ptd = NULL;
            }
        }
    }

    SetLastError(TL_LastError);

    return(ptd);
}

/***
*_ptiddata _getptd(void) - get per-thread data structure for the current thread
*
*Purpose:
*
*Entry:
*       unsigned long tid
*
*Exit:
*       success = pointer to _tiddata structure for the thread
*       failure = fatal runtime exit
*
*Exceptions:
*
*******************************************************************************/

_ptiddata __cdecl _getptd (
        void
        )
{
        _ptiddata ptd = _getptd_noexit();
        if (!ptd) {
            _amsg_exit(_RT_THREAD); /* write message and die */
        }
        return ptd;
}


/***
*void WINAPI _freefls(void *) - free up a per-fiber data structure
*
*Purpose:
*       Called from _freeptd, as a callback from deleting a fiber, and
*       from deleting an FLS index. This routine frees up the per-fiber
*       buffer associated with a fiber that is going away. The tiddata
*       structure itself is freed, but not until its subordinate buffers
*       are freed.
*
*Entry:
*       pointer to a per-fiber data block (malloc-ed memory)
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void
WINAPI
_freefls (
    void *data
    )

{

    _ptiddata ptd;
    pthreadlocinfo ptloci;
    pthreadmbcinfo ptmbci;

    /*
     * Free up the _tiddata structure & its malloc-ed buffers.
     */

    ptd = data;
    if (ptd != NULL) {
        if(ptd->_errmsg)
            _free_crt((void *)ptd->_errmsg);

        if(ptd->_namebuf0)
            _free_crt((void *)ptd->_namebuf0);

        if(ptd->_namebuf1)
            _free_crt((void *)ptd->_namebuf1);

        if(ptd->_asctimebuf)
            _free_crt((void *)ptd->_asctimebuf);

        if(ptd->_wasctimebuf)
            _free_crt((void *)ptd->_wasctimebuf);

        if(ptd->_gmtimebuf)
            _free_crt((void *)ptd->_gmtimebuf);

        if(ptd->_cvtbuf)
            _free_crt((void *)ptd->_cvtbuf);

        if (ptd->_pxcptacttab != _XcptActTab)
            _free_crt((void *)ptd->_pxcptacttab);

        _mlock(_MB_CP_LOCK);
        __try {
            if ( ((ptmbci = ptd->ptmbcinfo) != NULL) &&
                 (InterlockedDecrement(&(ptmbci->refcount)) == 0) &&
                 (ptmbci != &__initialmbcinfo) )
                _free_crt(ptmbci);
        }
        __finally {
            _munlock(_MB_CP_LOCK);
        }

        _mlock(_SETLOCALE_LOCK);

        __try {
            if ( (ptloci = ptd->ptlocinfo) != NULL )
            {
                __removelocaleref(ptloci);
                if ( (ptloci != __ptlocinfo) &&
                     (ptloci != &__initiallocinfo) &&
                     (ptloci->refcount == 0) )
                    __freetlocinfo(ptloci);
            }
        }
        __finally {
            _munlock(_SETLOCALE_LOCK);
        }

        _free_crt((void *)ptd);
    }
    return;
}

/***
*void _freeptd(_ptiddata) - free up a per-thread data structure
*
*Purpose:
*       Called from _endthread and from a DLL thread detach handler,
*       this routine frees up the per-thread buffer associated with a
*       thread that is going away.  The tiddata structure itself is
*       freed, but not until its subordinate buffers are freed.
*
*Entry:
*       pointer to a per-thread data block (malloc-ed memory)
*       If NULL, the pointer for the current thread is fetched.
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _freeptd (
        _ptiddata ptd
        )
{
        /*
         * Do nothing unless per-thread data has been allocated for this module!
         */

        if ( __flsindex != FLS_OUT_OF_INDEXES ) {

            /*
             * if parameter "ptd" is NULL, get the per-thread data pointer
             * Must NOT call _getptd because it will allocate one if none exists!
             * If FlsGetValue is NULL then ptd may not have been set
             */

            if ( ptd == NULL)
                ptd = __crtFlsGetValue(__flsindex);

            /*
             * Zero out the one pointer to the per-thread data block
             */

            __crtFlsSetValue(__flsindex, (LPVOID)0);

            _freefls(ptd);
        }
}


/***
*__threadid()     - Returns current thread ID
*__threadhandle() - Returns "pseudo-handle" for current thread
*
*Purpose:
*       The two function are simply do-nothing wrappers for the corresponding
*       Win32 APIs (GetCurrentThreadId and GetCurrentThread, respectively).
*
*Entry:
*       void
*
*Exit:
*       thread ID value
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP unsigned long __cdecl __threadid (
        void
        )
{
    return( GetCurrentThreadId() );
}

_CRTIMP uintptr_t __cdecl __threadhandle(
        void
        )
{
    return( (uintptr_t)GetCurrentThread() );
}
