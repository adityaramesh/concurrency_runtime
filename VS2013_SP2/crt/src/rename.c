/***
*rename.c - rename file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines rename() - rename a file
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <io.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>
#include <awint.h>

/***
*int rename(oldname, newname) - rename a file
*
*Purpose:
*       Renames a file to a new name -- no file with new name must
*       currently exist.
*
*Entry:
*       _TSCHAR *oldname -      name of file to rename
*       _TSCHAR *newname -      new name for file
*
*Exit:
*       returns 0 if successful
*       returns not 0 and sets errno if not successful
*
*Exceptions:
*
*******************************************************************************/

#ifndef _UNICODE

int __cdecl rename(
        const char *oldname,
        const char *newname )
{
    /*
       We want to use MoveFileExW but not MoveFileExA (or MoveFile).
       So convert the strings to Unicode representation and call _wrename.

       Note that minwin does not support OEM CP for conversion via
       the FileAPIs APIs. So we unconditionally use the ACP for conversion
    */
    int oldnamelen;
    int newnamelen;
    wchar_t *widenamesbuffer = NULL;
    wchar_t *oldnamew = NULL;
    wchar_t *newnamew = NULL;
    int retval;
    UINT codePage = CP_ACP;

#if !(defined (_CORESYS) || defined (_CRT_APP) || defined (_KERNELX))
    if (!__crtIsPackagedApp() && !AreFileApisANSI())
        codePage = CP_OEMCP;
#endif  /* !(defined (_CORESYS) || defined (_CRT_APP) || defined (_KERNELX)) */

    if (  ( (oldnamelen = MultiByteToWideChar(codePage, 0, oldname, -1, 0, 0) ) == 0 )
        || ( (newnamelen = MultiByteToWideChar(codePage, 0, newname, -1, 0, 0) ) == 0 ) )
    {
        _dosmaperr(GetLastError());
        return -1;
    }

    /* allocate enough space for both */
    if ( (widenamesbuffer = (wchar_t*)_malloc_crt( (oldnamelen+newnamelen) * sizeof(wchar_t) ) ) == NULL )
    {
        /* errno is set by _malloc */
        return -1;
    }

    /* now do the conversion */
    oldnamew = widenamesbuffer;
    newnamew = widenamesbuffer + oldnamelen;

    if (  ( MultiByteToWideChar(codePage, 0, oldname, -1, oldnamew, oldnamelen) == 0 )
        || ( MultiByteToWideChar(codePage, 0, newname, -1, newnamew, newnamelen) == 0 ) )
    {
        _free_crt(widenamesbuffer);
        _dosmaperr(GetLastError());
        return -1;
    }

    /* and event call the wide-character variant */
    retval = _wrename(oldnamew,newnamew);
    _free_crt(widenamesbuffer); /* _free_crt leaves errno alone if everything completes as expected */
    return retval;
}

#else  /* _UNICODE */

int __cdecl _wrename (
        const wchar_t *oldname,
        const wchar_t *newname
        )
{
        ULONG dosretval;

        if (!MoveFileExW(oldname, newname, MOVEFILE_COPY_ALLOWED /* allow moving to a different volume */))
            dosretval = GetLastError();
        else
            dosretval = 0;

        if (dosretval) {
            /* error occured -- map error code and return */
            _dosmaperr(dosretval);
            return -1;
        }

        return 0;
}

#endif  /* _UNICODE */
