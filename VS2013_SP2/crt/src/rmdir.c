/***
*rmdir.c - remove directory
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _rmdir() - remove a directory
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
*int _rmdir(path) - remove a directory
*
*Purpose:
*       deletes the directory speicifed by path.  The directory must
*       be empty, and it must not be the current working directory or
*       the root directory.
*
*Entry:
*       _TSCHAR *path - directory to remove
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/

#ifndef _UNICODE

int __cdecl _rmdir (
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
    retval = _wrmdir(pathw);

    _free_crt(pathw); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else  /* _UNICODE */

int __cdecl _wrmdir (
        const wchar_t *path
        )
{
        ULONG dosretval;

        /* ask OS to remove directory */

        if (!RemoveDirectoryW(path))
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
