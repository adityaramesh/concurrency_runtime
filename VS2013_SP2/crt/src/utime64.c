/***
*utime64.c - set modification time for a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Sets the access/modification times for a file.
*
*******************************************************************************/


#include <cruntime.h>
#include <sys/types.h>
#include <sys/utime.h>
#include <msdos.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#include <dos.h>
#include <oscalls.h>
#include <errno.h>
#include <stddef.h>
#include <share.h>
#include <internal.h>
#include <awint.h>

#include <stdio.h>
#include <tchar.h>

/***
*int _utime64(pathname, time) - set modification time for file
*
*Purpose:
*       Sets the modification time for the file specified by pathname.
*       Only the modification time from the __utimbuf64 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf64 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _tutime64 (
        const _TSCHAR *fname,
        struct __utimbuf64 *times
        )
{
        int fh;
        int retval;
        int errno_local = 0;

        _VALIDATE_RETURN( ( fname != NULL ), EINVAL, -1 )

        /* open file, fname, since filedate system call needs a handle.  Note
         * _utime definition says you must have write permission for the file
         * to change its time, so open file for write only.  Also, must force
         * it to open in binary mode so we dont remove ^Z's from binary files.
         */


        if (_tsopen_s(&fh, fname, _O_RDWR | _O_BINARY, _SH_DENYNO, 0) != 0)
                return(-1);

        retval = _futime64(fh, times);

        if ( retval == -1 )
        {
            errno_local = errno;
        }

        _close(fh);

        if ( retval == -1 )
        {
            errno = errno_local;
        }

        return(retval);
}

#ifndef _UNICODE

/***
*int _futime64(fh, time) - set modification time for open file
*
*Purpose:
*       Sets the modification time for the open file specified by fh.
*       Only the modification time from the __utimbuf64 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf64 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _futime64 (
        int fh,
        struct __utimbuf64 *times
        )
{
        struct tm tmb;

        SYSTEMTIME SystemTime;
        FILETIME LastWriteTime;
        FILETIME LastAccessTime;
        struct __utimbuf64 deftimes;
#if !defined(_KERNELX) && defined(_CRT_APP)
        FILE_BASIC_INFO fileInfo;
#else  /* !defined(_KERNELX) && defined(_CRT_APP) */
        FILETIME LocalFileTime;
#endif  /* !defined(_KERNELX) && defined(_CRT_APP) */

        _CHECK_FH_RETURN( fh, EBADF, -1 );

        _VALIDATE_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_RETURN((_osfile(fh) & FOPEN), EBADF, -1);

        if (times == NULL) {
                _time64(&deftimes.modtime);
                deftimes.actime = deftimes.modtime;
                times = &deftimes;
        }

        if (_localtime64_s(&tmb, &times->modtime) != 0) {
                errno = EINVAL;
                return(-1);
        }

        SystemTime.wYear   = (WORD)(tmb.tm_year + 1900);
        SystemTime.wMonth  = (WORD)(tmb.tm_mon + 1);
        SystemTime.wDay    = (WORD)(tmb.tm_mday);
        SystemTime.wHour   = (WORD)(tmb.tm_hour);
        SystemTime.wMinute = (WORD)(tmb.tm_min);
        SystemTime.wSecond = (WORD)(tmb.tm_sec);
        SystemTime.wMilliseconds = 0;

#if !defined(_KERNELX) && defined(_CRT_APP)
        if ( !TzSpecificLocalTimeToSystemTime( NULL, &SystemTime, &SystemTime) ||
             !SystemTimeToFileTime( &SystemTime, &LastWriteTime ) )
#else  /* !defined(_KERNELX) && defined(_CRT_APP) */
        if ( !SystemTimeToFileTime( &SystemTime, &LocalFileTime ) ||
             !LocalFileTimeToFileTime( &LocalFileTime, &LastWriteTime ) )
#endif  /* !defined(_KERNELX) && defined(_CRT_APP) */
        {
                errno = EINVAL;
                return(-1);
        }

        if (_localtime64_s(&tmb, &times->actime) != 0) {
                errno = EINVAL;
                return(-1);
        }

        SystemTime.wYear   = (WORD)(tmb.tm_year + 1900);
        SystemTime.wMonth  = (WORD)(tmb.tm_mon + 1);
        SystemTime.wDay    = (WORD)(tmb.tm_mday);
        SystemTime.wHour   = (WORD)(tmb.tm_hour);
        SystemTime.wMinute = (WORD)(tmb.tm_min);
        SystemTime.wSecond = (WORD)(tmb.tm_sec);
        SystemTime.wMilliseconds = 0;

#if !defined(_KERNELX) && defined(_CRT_APP)
        if ( !TzSpecificLocalTimeToSystemTime( NULL, &SystemTime, &SystemTime) ||
             !SystemTimeToFileTime( &SystemTime, &LastAccessTime ) )
        {
                errno = EINVAL;
                return(-1);
        }

        /* get file date information */
        if (!GetFileInformationByHandleEx((HANDLE)_get_osfhandle(fh),
                                FileBasicInfo,
                                &fileInfo,
                                sizeof(fileInfo)
                               ))
        {
                errno = EINVAL;
                return(-1);
        }

        /* set the date via the filedate system call and return. failing
         * this call implies the new file times are not supported by the
         * underlying file system.
         */

        fileInfo.LastAccessTime.LowPart  = LastAccessTime.dwLowDateTime;
        fileInfo.LastAccessTime.HighPart = LastAccessTime.dwHighDateTime;
        fileInfo.LastWriteTime.LowPart   = LastWriteTime.dwLowDateTime;
        fileInfo.LastWriteTime.HighPart  = LastWriteTime.dwHighDateTime;
        fileInfo.FileAttributes = 0;

        if (!SetFileInformationByHandle((HANDLE)_get_osfhandle(fh),
                                FileBasicInfo,
                                &fileInfo,
                                sizeof(fileInfo)
                               ))
        {
                errno = EINVAL;
                return(-1);
        }
#else  /* !defined(_KERNELX) && defined(_CRT_APP) */
        if ( !SystemTimeToFileTime( &SystemTime, &LocalFileTime ) ||
             !LocalFileTimeToFileTime( &LocalFileTime, &LastAccessTime ) )
        {
                errno = EINVAL;
                return(-1);
        }

        /* set the date via the filedate system call and return. failing
         * this call implies the new file times are not supported by the
         * underlying file system.
         */

        if (!SetFileTime((HANDLE)_get_osfhandle(fh),
                                NULL,
                                &LastAccessTime,
                                &LastWriteTime
                               ))
        {
                errno = EINVAL;
                return(-1);
        }
#endif  /* !defined(_KERNELX) && defined(_CRT_APP) */

        return(0);
}

#endif  /* _UNICODE */

