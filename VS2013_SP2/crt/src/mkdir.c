/***
*mkdir.c - make directory
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines function _mkdir() - make a directory
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <direct.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>

/***
*int _mkdir(path) - make a directory
*
*Purpose:
*       creates a new directory with the specified name
*
*Entry:
*       _TSCHAR *path - name of new directory
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/
#ifndef _UNICODE

int __cdecl _mkdir (
        const char *path
        )
{
    wchar_t* pathw = NULL;
    int retval;

    if (path)
    {
        if (!__copy_path_to_wide_string(path, &pathw))
            return -1;
    }

    /* call the wide-char variant */
    retval = _wmkdir(pathw);

    _free_crt(pathw); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else  /* _UNICODE */

int __cdecl _wmkdir (
        const wchar_t *path
        )
{
        ULONG dosretval;

        /* ask OS to create directory */

        if (!CreateDirectoryW(path, (LPSECURITY_ATTRIBUTES)NULL))
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
