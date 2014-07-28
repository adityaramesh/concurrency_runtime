/***
*fullpath.c -
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose: contains the function _fullpath which makes an absolute path out
*       of a relative path. i.e.  ..\pop\..\main.c => c:\src\main.c if the
*       current directory is c:\src\src
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <direct.h>
#include <errno.h>
#include <stdlib.h>
#include <internal.h>
#include <tchar.h>
#include <windows.h>
#include <awint.h>

/***
*_TSCHAR *_fullpath( _TSCHAR *buf, const _TSCHAR *path, maxlen );
*
*Purpose:
*
*       _fullpath - combines the current directory with path to form
*       an absolute path. i.e. _fullpath takes care of .\ and ..\
*       in the path.
*
*       The result is placed in buf. If the length of the result
*       is greater than maxlen NULL is returned, otherwise
*       the address of buf is returned.
*
*       If buf is NULL then a buffer is malloc'ed and maxlen is
*       ignored. If there are no errors then the address of this
*       buffer is returned.
*
*       If path specifies a drive, the curent directory of this
*       drive is combined with path. If the drive is not valid
*       and _fullpath needs the current directory of this drive
*       then NULL is returned.  If the current directory of this
*       non existant drive is not needed then a proper value is
*       returned.
*       For example:  path = "z:\\pop" does not need z:'s current
*       directory but path = "z:pop" does.
*
*
*
*Entry:
*       _TSCHAR *buf  - pointer to a buffer maintained by the user;
*       _TSCHAR *path - path to "add" to the current directory
*       int maxlen - length of the buffer pointed to by buf
*
*Exit:
*       Returns pointer to the buffer containing the absolute path
*       (same as buf if non-NULL; otherwise, malloc is
*       used to allocate a buffer)
*
*Exceptions:
*
*******************************************************************************/

#ifdef _DEBUG

_TSCHAR * __cdecl _tfullpath (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen
        )
{
    return _tfullpath_dbg(UserBuf, path, maxlen, _NORMAL_BLOCK, NULL, 0);
}

_TSCHAR * __cdecl _tfullpath_dbg (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )

#else  /* _DEBUG */

_TSCHAR * __cdecl _tfullpath (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen
        )

#endif  /* _DEBUG */
{
        _TSCHAR *buf;
#if !defined(_CRT_APP) || defined(_KERNELX)
        _TSCHAR *pfname;
#endif  /* _CRT_APP */
        unsigned long count;

        if ( !path || !*path )  /* no work to do */
#if !defined (_CRT_APP)
            return( _tgetcwd( UserBuf, (int)maxlen ) );
#else  /* !defined (_CRT_APP) */
            return( _T("") );
#endif  /* !defined (_CRT_APP) */


        /* allocate buffer if necessary */

        if ( !UserBuf ) {
#if defined (_CRT_APP) && !defined (_KERNELX)
#if defined (_UNICODE)
                        count = __crtGetFullPathNameWinRTW(path, 0, NULL);
#else  /* defined (_UNICODE) */
                        count = __crtGetFullPathNameWinRTA(path, 0, NULL);
#endif  /* defined (_UNICODE) */
#else  /* defined (_CRT_APP) && !defined (_KERNELX) */
            count = GetFullPathName(path,0,NULL,NULL);
#endif  /* defined (_CRT_APP) */

            if(count == 0) {
                _dosmaperr( GetLastError() );
                return( NULL );
            }

            maxlen = __max(maxlen, count);

            // overflow check

            if (maxlen > SIZE_MAX / sizeof(_TSCHAR)) {
               errno = EINVAL;
               return( NULL );
            }

#ifdef _DEBUG
            if ( !(buf = _calloc_dbg(maxlen, sizeof(_TSCHAR), nBlockUse, szFileName, nLine)) ) {
#else  /* _DEBUG */
            if ( !(buf = calloc(maxlen, sizeof(_TSCHAR))) ) {
#endif  /* _DEBUG */
                errno = ENOMEM;
                return( NULL );
            }
        }
        else
        {
            _VALIDATE_RETURN( (maxlen > 0), EINVAL, NULL);
            buf = UserBuf;
        }

#if defined (_CRT_APP) && !defined (_KERNELX)
#if defined (_UNICODE)
                count = __crtGetFullPathNameWinRTW(path, (int)maxlen, buf);
#else  /* defined (_UNICODE) */
                count = __crtGetFullPathNameWinRTA(path, (int)maxlen, buf);
#endif  /* defined (_UNICODE) */
#else  /* defined (_CRT_APP) && !defined (_KERNELX) */
        count = GetFullPathName ( path,
                                  (int)maxlen,
                                  buf,
                                  &pfname );
#endif  /* defined (_CRT_APP) */

        if ( count >= maxlen ) {
            if ( !UserBuf )
                free(buf);
            errno = ERANGE;
            return( NULL );
        }
        else if ( count == 0 ) {
            if ( !UserBuf )
                free(buf);
            _dosmaperr( GetLastError() );
            return( NULL );
        }

        return( buf );

}
