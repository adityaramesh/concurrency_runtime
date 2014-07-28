/***
*findf64.c - C find file functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _findfirst64() and _findnext64(), OR
*       defines _findfirst64i32() and _findnext64i32() depending
*       on the state of _USE_INT64 flag
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <errno.h>
#include <io.h>
#include <time.h>
#include <ctime.h>
#include <string.h>
#include <stdlib.h>
#include <internal.h>
#include <tchar.h>
#include <dbgint.h>

#ifndef _WIN32
#error ERROR - ONLY WIN32 TARGET SUPPORTED!
#endif  /* _WIN32 */

#ifndef _USE_INT64
#define _USE_INT64 1
#endif  /* _USE_INT64 */

__time64_t __cdecl __time64_t_from_ft(FILETIME * pft);
#if _USE_INT64
BOOL __cdecl _copyfinddata64(struct __finddata64_t* pfd, const struct _wfinddata64_t* wfd);
#else  /* _USE_INT64 */
BOOL __cdecl _copyfinddata64i32(struct _finddata64i32_t* pfd, const struct _wfinddata64i32_t* wfd);
#endif  /* _USE_INT64 */


/***
*long _findfirst(wildspec, finddata) - Find first matching file
*
*Purpose:
*       Finds the first file matching a given wild card filespec and
*       returns data about the file.
*
*Entry:
*       char * wild - file spec optionally containing wild cards
*
*       struct _finddata64_t * finddata - structure to receive file data
*
*Exit:
*       Good return:
*       Unique handle identifying the group of files matching the spec
*
*       Error return:
*       Returns -1 and errno is set to error value
*
*Exceptions:
*       None.
*
*******************************************************************************/
#ifdef _UNICODE

#if _USE_INT64

intptr_t __cdecl _wfindfirst64(
        const wchar_t * szWild,
        struct _wfinddata64_t * pfd
        )

#else  /* _USE_INT64 */

intptr_t __cdecl _wfindfirst64i32(
        const wchar_t * szWild,
        struct _wfinddata64i32_t * pfd
        )

#endif  /* _USE_INT64 */

{
        WIN32_FIND_DATAW wfd;
        HANDLE          hFile;
        DWORD           err;

        _VALIDATE_RETURN( (pfd != NULL), EINVAL, -1);

        /* We assert to make sure the underlying Win32 struct WIN32_FIND_DATA's
        cFileName member doesn't have an array size greater than ours */

        _VALIDATE_RETURN( (sizeof(pfd->name) <= sizeof(wfd.cFileName)), ENOMEM, -1);
        _VALIDATE_RETURN( (szWild != NULL), EINVAL, -1);

        if ((hFile = FindFirstFileExW(szWild, FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, 0)) == INVALID_HANDLE_VALUE) {
            err = GetLastError();
            switch (err) {
                case ERROR_NO_MORE_FILES:
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND:
                    errno = ENOENT;
                    break;

                case ERROR_NOT_ENOUGH_MEMORY:
                    errno = ENOMEM;
                    break;

                default:
                    errno = EINVAL;
                    break;
            }
            return (-1);
        }

        pfd->attrib       = (wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
                            ? 0 : wfd.dwFileAttributes;
        pfd->time_create  = __time64_t_from_ft(&wfd.ftCreationTime);
        pfd->time_access  = __time64_t_from_ft(&wfd.ftLastAccessTime);
        pfd->time_write   = __time64_t_from_ft(&wfd.ftLastWriteTime);
#if _USE_INT64
        pfd->size         = ((__int64)(wfd.nFileSizeHigh)) * (0x100000000i64) +
                            (__int64)(wfd.nFileSizeLow);
#else  /* _USE_INT64 */
    pfd->size         = wfd.nFileSizeLow;
#endif  /* _USE_INT64 */

        _ERRCHECK(wcscpy_s(pfd->name, _countof(pfd->name), wfd.cFileName));

        return ((intptr_t)hFile);
}

#else  /* _UNICODE */

#if _USE_INT64

intptr_t __cdecl _findfirst64(
        const char * szWild,
        struct __finddata64_t * pfd
        )

#else  /* _USE_INT64 */

intptr_t __cdecl _findfirst64i32(
        const char * szWild,
        struct _finddata64i32_t * pfd
        )

#endif  /* _USE_INT64 */

{
#if _USE_INT64
        struct _wfinddata64_t wfd;
#else  /* _USE_INT64 */
        struct _wfinddata64i32_t wfd;
#endif  /* _USE_INT64 */
        wchar_t* wszWild = NULL;
        char* name = NULL;
        intptr_t retval;

        if (!__copy_path_to_wide_string(szWild, &wszWild))
            return -1;

#if _USE_INT64
        retval = _wfindfirst64(wszWild, &wfd);
#else  /* _USE_INT64 */
        retval = _wfindfirst64i32(wszWild, &wfd);
#endif  /* _USE_INT64 */

        _free_crt(wszWild); /* _free_crt leaves errno alone if everything completes as expected */

        if (retval != -1) {
#if _USE_INT64
            if (!_copyfinddata64(pfd, &wfd))
#else  /* _USE_INT64 */
            if (!_copyfinddata64i32(pfd, &wfd))
#endif  /* _USE_INT64 */
                return -1;
        }

        return retval;
}
#endif  /* _UNICODE */

/***
*int _findnext(hfind, finddata) - Find next matching file
*
*Purpose:
*       Finds the next file matching a given wild card filespec and
*       returns data about the file.
*
*Entry:
*       hfind - handle from _findfirst
*
*       struct __finddata64_t * finddata - structure to receive file data
*
*Exit:
*       Good return:
*       0 if file found
*       -1 if error or file not found
*       errno set
*
*Exceptions:
*       None.
*
*******************************************************************************/
#ifdef _UNICODE

#if _USE_INT64

int __cdecl _wfindnext64(intptr_t hFile, struct _wfinddata64_t * pfd)

#else  /* _USE_INT64 */

int __cdecl _wfindnext64i32(intptr_t hFile, struct _wfinddata64i32_t * pfd)

#endif  /* _USE_INT64 */

{
        WIN32_FIND_DATAW wfd;
        DWORD           err;

        _VALIDATE_RETURN( ((HANDLE)hFile != INVALID_HANDLE_VALUE), EINVAL, -1);
        _VALIDATE_RETURN( (pfd != NULL), EINVAL, -1);
        _VALIDATE_RETURN( (sizeof(pfd->name) <= sizeof(wfd.cFileName)), ENOMEM, -1);

        if (!FindNextFileW((HANDLE)hFile, &wfd)) {
            err = GetLastError();
            switch (err) {
                case ERROR_NO_MORE_FILES:
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND:
                    errno = ENOENT;
                    break;

                case ERROR_NOT_ENOUGH_MEMORY:
                    errno = ENOMEM;
                    break;

                default:
                    errno = EINVAL;
                    break;
            }
            return (-1);
        }

        pfd->attrib       = (wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
                            ? 0 : wfd.dwFileAttributes;
        pfd->time_create  = __time64_t_from_ft(&wfd.ftCreationTime);
        pfd->time_access  = __time64_t_from_ft(&wfd.ftLastAccessTime);
        pfd->time_write   = __time64_t_from_ft(&wfd.ftLastWriteTime);
#if _USE_INT64
        pfd->size         = ((__int64)(wfd.nFileSizeHigh)) * (0x100000000i64) +
                            (__int64)(wfd.nFileSizeLow);
#else  /* _USE_INT64 */
        pfd->size         = wfd.nFileSizeLow;
#endif  /* _USE_INT64 */

        _ERRCHECK(wcscpy_s(pfd->name, _countof(pfd->name), wfd.cFileName));

        return (0);
}

#else  /* _UNICODE */

#if _USE_INT64

int __cdecl _findnext64(intptr_t hFile, struct __finddata64_t * pfd)

#else  /* _USE_INT64 */

int __cdecl _findnext64i32(intptr_t hFile, struct _finddata64i32_t * pfd)

#endif  /* _USE_INT64 */

{
#if _USE_INT64
        struct _wfinddata64_t wfd;
#else  /* _USE_INT64 */
        struct _wfinddata64i32_t wfd;
#endif  /* _USE_INT64 */
        char* name = NULL;
        int retval;

#if _USE_INT64
        retval = _wfindnext64(hFile, &wfd);
#else  /* _USE_INT64 */
        retval = _wfindnext64i32(hFile, &wfd);
#endif  /* _USE_INT64 */

        if (retval != -1) {
#if _USE_INT64
            if (!_copyfinddata64(pfd, &wfd))
#else  /* _USE_INT64 */
            if (!_copyfinddata64i32(pfd, &wfd))
#endif  /* _USE_INT64 */
                return -1;
        }

    return retval;
}

/***
*BOOL __cdecl _copyfinddata64(pfd,wfd) - copies data from the wide-char find data
                                          struct to ANSI find data struct.
*
*******************************************************************************/
#if _USE_INT64
BOOL __cdecl _copyfinddata64(struct __finddata64_t* pfd, const struct _wfinddata64_t* wfd)
#else  /* _USE_INT64 */
BOOL __cdecl _copyfinddata64i32(struct _finddata64i32_t* pfd, const struct  _wfinddata64i32_t* wfd)
#endif  /* _USE_INT64 */
{
        char* name = NULL;

        if (!__copy_to_char(wfd->name, &name))
            return FALSE;

        _ERRCHECK(strcpy_s(pfd->name, _countof(pfd->name), name));
        _free_crt(name);

        pfd->attrib       = wfd->attrib;
        pfd->time_create  = wfd->time_create;
        pfd->time_access  = wfd->time_access;
        pfd->time_write   = wfd->time_write;
        pfd->size         = wfd->size;

        return TRUE;
}

#if !_USE_INT64

/***
*time64_t __time64_t_from_ft(ft) - convert Win32 file time to Xenix time
*
*Purpose:
*       converts a Win32 file time value to Xenix time_t
*
*       Note: We cannot directly use the ft value. In Win32, the file times
*       returned by the API are ambiguous. In Windows NT, they are UTC. In
*       Win32S, and probably also Win32C, they are local time values. Thus,
*       the value in ft must be converted to a local time value (by an API)
*       before we can use it.
*
*Entry:
*       int yr, mo, dy -        date
*       int hr, mn, sc -        time
*
*Exit:
*       returns Xenix time value
*
*Exceptions:
*
*******************************************************************************/

__time64_t __cdecl __time64_t_from_ft(FILETIME * pft)
{
        SYSTEMTIME st;
        SYSTEMTIME stUTC;

        /* 0 FILETIME returns a -1 time_t */

        if (!pft->dwLowDateTime && !pft->dwHighDateTime) {
            return ((__time64_t)-1);
        }

        /*
         * Convert to a broken down local time value
         */
//TODO - KERNELX
#ifndef _KERNELX
        if ( !FileTimeToSystemTime(pft, &stUTC) ||
             !SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &st) )
        {
            return ((__time64_t)-1);
        }
#else  /* _KERNELX */
            stUTC;
            return ((__time64_t)-1);
#endif  /* _KERNELX */

        return ( __loctotime64_t(st.wYear,
                                 st.wMonth,
                                 st.wDay,
                                 st.wHour,
                                 st.wMinute,
                                 st.wSecond,
                                 -1) );
}

#endif  /* !_USE_INT64 */

#endif  /* _UNICODE */
