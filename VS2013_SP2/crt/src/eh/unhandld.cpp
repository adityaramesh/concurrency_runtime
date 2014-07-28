/***
*unhandld.cxx - Wrapper to call terminate() when an exception goes unhandled.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Wrapper to call terminate() when an exception goes unhandled.
****/

#include <windows.h>
#include <ehdata.h>
#include <eh.h>
#include <ehhooks.h>
#include <ehassert.h>
#include <awint.h>
#include <internal.h>
#include <stdlib.h>

#pragma hdrstop

#include <sect_attribs.h>

extern "C" int  __cdecl __CxxSetUnhandledExceptionFilter(void);

/////////////////////////////////////////////////////////////////////////////
//
// __CxxUnhandledExceptionFilter - if the exception is ours, call terminate();
//
// Returns:
//      If the exception was MSVC C++ EH, does not return.
//      If the previous filter was NULL, returns EXCEPTION_CONTINUE_SEARCH.
//      Otherwise returns value returned by previous filter.
//
LONG WINAPI __CxxUnhandledExceptionFilter(
        LPEXCEPTION_POINTERS pPtrs
        )
{
        if (PER_IS_MSVC_PURE_OR_NATIVE_EH((EHExceptionRecord*)(pPtrs->ExceptionRecord))) {
                terminate();            // Does not return
                return EXCEPTION_EXECUTE_HANDLER;
        }
        return EXCEPTION_CONTINUE_SEARCH;
}


/////////////////////////////////////////////////////////////////////////////
//
// __CxxSetUnhandledExceptionFilter - sets unhandled exception filter to be
// __CxxUnhandledExceptionFilter.
//
// Returns:
//      Returns 0 to indicate no error.
//

extern "C" int __cdecl __CxxSetUnhandledExceptionFilter(void)
{
    // ignore return value on purpose
    // we dont want to save the previous UEF, because we aren't using nor restoring it
#ifndef _CRT_APP
    __crtSetUnhandledExceptionFilter(&__CxxUnhandledExceptionFilter);
#endif
    return 0;
}
