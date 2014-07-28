/***
*unlink.c - unlink a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines unlink() - unlink a file
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <stdio.h>
#include <tchar.h>
#include <malloc.h>
#include <dbgint.h>

/***
*int _unlink(path) - unlink(delete) the given file
*
*Purpose:
*       This version deletes the given file because there is no
*       distinction between a linked file and non-linked file.
*
*       NOTE: remove() is an alternative entry point to the _unlink()
*       routine* interface is identical.
*
*Entry:
*       _TSCHAR *path - file to unlink/delete
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/
#ifndef _UNICODE

int __cdecl remove (
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
    retval = _wremove(pathw);

    _free_crt(pathw); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else  /* _UNICODE */

int __cdecl _wremove (
        const wchar_t *path
        )
{
        ULONG dosretval;

        if (!DeleteFileW(path))
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

int __cdecl _tunlink (
        const _TSCHAR *path
        )
{
        /* remove is synonym for unlink */
        return _tremove(path);
}
