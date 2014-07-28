/***
*chmod.c - change file attributes
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _chmod() - change file attributes
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <stdlib.h>
#include <internal.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <tchar.h>
#include <dbgint.h>

/***
*int _chmod(path, mode) - change file mode
*
*Purpose:
*       Changes file mode permission setting to that specified in
*       mode.  The only XENIX mode bit supported is user write.
*
*Entry:
*       _TSCHAR *path - file name
*       int mode - mode to change to
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if not successful
*
*Exceptions:
*
*******************************************************************************/
#ifndef _UNICODE

int __cdecl _chmod (
        const char *path,
        int mode
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
    retval = _wchmod(pathw, mode);

    _free_crt(pathw); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else  /* _UNICODE */

int __cdecl _wchmod (
        const wchar_t *path,
        int mode
        )
{
        WIN32_FILE_ATTRIBUTE_DATA attr_data;

        _VALIDATE_CLEAR_OSSERR_RETURN((path != NULL), EINVAL, -1);

        if (!GetFileAttributesExW((LPWSTR)path, GetFileExInfoStandard, (void*) &attr_data)) {
                /* error occured -- map error code and return */
                _dosmaperr(GetLastError());
                return -1;
        }

        if (mode & _S_IWRITE) {
                /* clear read only bit */
                attr_data.dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
        }
        else {
                /* set read only bit */
                attr_data.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
        }

        /* set new attribute */
        if (!SetFileAttributesW((LPTSTR)path, attr_data.dwFileAttributes)) {
                /* error occured -- map error code and return */
                _dosmaperr(GetLastError());
                return -1;
        }

        return 0;
}

#endif  /* _UNICODE */
