/***
*stat.c - get file status
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _stat() - get file status
*
*******************************************************************************/

#include <cruntime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <msdos.h>
#include <oscalls.h>
#include <string.h>
#include <internal.h>
#include <stdlib.h>
#include <direct.h>
#include <mbstring.h>
#include <tchar.h>
#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <dbgint.h>


#define ISSLASH(a)  ((a) == _T('\\') || (a) == _T('/'))


/*
 * Local routine which returns true if the argument is a UNC name
 * specifying the root name of a share, such as '\\server\share\'.
 */

static int IsRootUNCName(const wchar_t *path);
static wchar_t * _wfullpath_helper(wchar_t * ,const wchar_t *,size_t , wchar_t **);


/***
*unsigned __wdtoxmode(attr, name) -
*
*Purpose:
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/
#ifdef _UNICODE

#ifdef _USE_INT64

extern unsigned short __cdecl __wdtoxmode(int, const _TSCHAR *);

#else  /* _USE_INT64 */


unsigned short __cdecl __wdtoxmode (
    int attr,
    const wchar_t *name
    )
{
    unsigned short uxmode;
    unsigned dosmode;
    const wchar_t *p;

    dosmode = attr & 0xff;
    if ((p = name)[1] == _T(':'))
        p += 2;

    /* check to see if this is a directory - note we must make a special
    * check for the root, which DOS thinks is not a directory
    */

    uxmode = (unsigned short)
             (((ISSLASH(*p) && !p[1]) || (dosmode & A_D) || !*p)
             ? _S_IFDIR|_S_IEXEC : _S_IFREG);

    /* If attribute byte does not have read-only bit, it is read-write */

    uxmode |= (dosmode & A_RO) ? _S_IREAD : (_S_IREAD|_S_IWRITE);

    /* see if file appears to be executable - check extension of name */

    if (p = wcsrchr(name, _T('.'))) {
        if ( !_wcsicmp(p, _T(".exe")) ||
             !_wcsicmp(p, _T(".cmd")) ||
             !_wcsicmp(p, _T(".bat")) ||
             !_wcsicmp(p, _T(".com")) )
            uxmode |= _S_IEXEC;
    }

    /* propagate user read/write/execute bits to group/other fields */

    uxmode |= (uxmode & 0700) >> 3;
    uxmode |= (uxmode & 0700) >> 6;

    return(uxmode);
}

#endif  /* _USE_INT64 */

#endif  /* _UNICODE */

/***
*int _stat(name, buf) - get file status info
*
*Purpose:
*       _stat obtains information about the file and stores it in the
*       structure pointed to by buf.
*
*       Note: We cannot directly use the file time stamps returned in the
*       WIN32_FIND_DATA structure. The values are supposedly in system time
*       and system time is ambiguously defined (it is UTC for Windows NT, local
*       time for Win32S and probably local time for Win32C). Therefore, these
*       values must be converted to local time before than can be used.
*
*Entry:
*       _TSCHAR *name -    pathname of given file
*       struct _stat *buffer - pointer to buffer to store info in
*
*Exit:
*       fills in structure pointed to by buffer
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/

#ifdef _USE_INT64
 #define _STAT_FUNC _stat32i64
 #define _W_STAT_FUNC _wstat32i64
 #define _STAT_STRUCT _stat32i64
 #define _FSTAT_FUNC _fstat32i64
#else  /* _USE_INT64 */
 #define _STAT_FUNC _stat32
 #define _W_STAT_FUNC _wstat32
 #define _STAT_STRUCT _stat32
 #define _FSTAT_FUNC _fstat32
#endif  /* _USE_INT64 */

#ifndef _UNICODE

int __cdecl _STAT_FUNC (
    const char *name,
    struct _STAT_STRUCT *buf
    )
{
    wchar_t* namew = NULL;
    int retval;

    if (name)
    {
        if (!__copy_path_to_wide_string(name, &namew))
            return -1;
    }

    /* call the wide-char variant */
    retval = _W_STAT_FUNC(namew, buf);

    _free_crt(namew); /* _free_crt leaves errno alone if everything completes as expected */

    return retval;
}

#else  /* _UNICODE */

int __cdecl _W_STAT_FUNC (
    const wchar_t *name,
    struct _STAT_STRUCT *buf
    )
{
    wchar_t *  path;
    wchar_t    pathbuf[ _MAX_PATH ];
    int        drive = -1; /* A: = 1, B: = 2, etc. */
    HANDLE     findhandle;
    WIN32_FIND_DATA findbuf;
    int retval = 0;

    _VALIDATE_CLEAR_OSSERR_RETURN( (name != NULL), EINVAL, -1);
    _VALIDATE_CLEAR_OSSERR_RETURN( (buf != NULL), EINVAL, -1);

    /* Don't allow wildcards to be interpreted by system */
    if (wcspbrk(name, L"?*"))
    {
        errno = ENOENT;
        _doserrno = E_nofile;
        return(-1);
    }

    /* Try to get disk from name.  If none, get current disk.  */

    if (name[1] == _T(':'))
    {
        if ( *name && !name[2] )
        {
            errno = ENOENT;             /* return an error if name is   */
            _doserrno = E_nofile;       /* just drive letter then colon */
            return( -1 );
        }
        drive = towlower(*name) - _T('a') + 1;
    }
#if !defined (_CRT_APP)
    /* TODO: Make sure drive is derived correcly if using relative name */
    else
    {
        drive = _getdrive();
    }
#endif  /* !defined (_CRT_APP) */

    /* Call Find Match File */
    findhandle = FindFirstFileExW(name, FindExInfoStandard, &findbuf, FindExSearchNameMatch, NULL, 0);
    if ( findhandle == INVALID_HANDLE_VALUE )
    {
        wchar_t * pBuf = NULL;
        if ( !( wcspbrk(name, L"./\\") &&
             (path = _wfullpath_helper( pathbuf, name, _MAX_PATH, &pBuf )) &&
             /* root dir. ('C:\') or UNC root dir. ('\\server\share\') */
             ((wcslen( path ) == 3) || IsRootUNCName(path))
#ifndef _CRT_APP
             && (GetDriveType( path ) > 1)
#endif  /* _CRT_APP */
             ) )
        {
            if(pBuf)
                free(pBuf);

            errno = ENOENT;
            _doserrno = E_nofile;
            return( -1 );
        }

        if(pBuf)
        {
            free(pBuf);
        }

        /*
         * Root directories (such as C:\ or \\server\share\ are fabricated.
         */

        findbuf.dwFileAttributes = A_D;
        findbuf.nFileSizeHigh = 0;
        findbuf.nFileSizeLow = 0;
        findbuf.cFileName[0] = _T('\0');

        buf->st_mtime = __loctotime32_t(1980,1,1,0,0,0, -1);
        buf->st_atime = buf->st_mtime;
        buf->st_ctime = buf->st_mtime;
    }
    else if ( (findbuf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
              (findbuf.dwReserved0 == IO_REPARSE_TAG_SYMLINK) )
    {
        /* if the file is a symbolic link, then use fstat to fill the info in the _stat struct */
        int fd = -1;
        errno_t e;
        int oflag = _O_RDONLY;

        if (findbuf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            oflag |= _O_OBTAIN_DIR;
        }

        e = _wsopen_s(&fd, name, oflag, _SH_DENYNO, 0 /* ignored */);
        if (e != 0 || fd == -1)
        {
            errno = ENOENT;
            _doserrno = E_nofile;
            return -1;
        }

        retval = _FSTAT_FUNC(fd, buf);
        _close(fd);
        FindClose( findhandle );

        return retval;
    }
    else
    {
        SYSTEMTIME SystemTime;
#ifdef _KERNELX
        FILETIME LocalFTime;
#endif  /* _KERNELX */

        if ( findbuf.ftLastWriteTime.dwLowDateTime ||
             findbuf.ftLastWriteTime.dwHighDateTime )
        {
#ifndef _KERNELX
            if ( !FileTimeToSystemTime( &findbuf.ftLastWriteTime, &SystemTime ) ||
                 !SystemTimeToTzSpecificLocalTime( NULL, &SystemTime, &SystemTime ) )
#else  /* _KERNELX */
            if ( !FileTimeToLocalFileTime( &findbuf.ftLastWriteTime,
                                           &LocalFTime )            ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
#endif  /* _KERNELX */
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_mtime = __loctotime32_t( SystemTime.wYear,
                                             SystemTime.wMonth,
                                             SystemTime.wDay,
                                             SystemTime.wHour,
                                             SystemTime.wMinute,
                                             SystemTime.wSecond,
                                             -1 );
        }
        else
        {
            buf->st_mtime = 0;
        }

        if ( findbuf.ftLastAccessTime.dwLowDateTime ||
             findbuf.ftLastAccessTime.dwHighDateTime )
        {
#ifndef _KERNELX
            if ( !FileTimeToSystemTime( &findbuf.ftLastAccessTime, &SystemTime ) ||
                 !SystemTimeToTzSpecificLocalTime( NULL, &SystemTime, &SystemTime ) )
#else  /* _KERNELX */
            if ( !FileTimeToLocalFileTime( &findbuf.ftLastAccessTime,
                                           &LocalFTime )                ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
#endif  /* _KERNELX */
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_atime = __loctotime32_t( SystemTime.wYear,
                                             SystemTime.wMonth,
                                             SystemTime.wDay,
                                             SystemTime.wHour,
                                             SystemTime.wMinute,
                                             SystemTime.wSecond,
                                             -1 );
        }
        else
        {
            buf->st_atime = buf->st_mtime;
        }

        if ( findbuf.ftCreationTime.dwLowDateTime ||
             findbuf.ftCreationTime.dwHighDateTime )
        {
#ifndef _KERNELX
            if ( !FileTimeToSystemTime( &findbuf.ftCreationTime, &SystemTime ) ||
                 !SystemTimeToTzSpecificLocalTime( NULL, &SystemTime, &SystemTime ) )
#else  /* _KERNELX */
            if ( !FileTimeToLocalFileTime( &findbuf.ftCreationTime,
                                           &LocalFTime )                ||
                 !FileTimeToSystemTime( &LocalFTime, &SystemTime ) )
#endif  /* _KERNELX */
            {
                _dosmaperr( GetLastError() );
                FindClose( findhandle );
                return( -1 );
            }

            buf->st_ctime = __loctotime32_t( SystemTime.wYear,
                                             SystemTime.wMonth,
                                             SystemTime.wDay,
                                             SystemTime.wHour,
                                             SystemTime.wMinute,
                                             SystemTime.wSecond,
                                             -1 );
        }
        else
        {
            buf->st_ctime = buf->st_mtime;
        }

        FindClose(findhandle);
    }

    /* Fill in buf */

    buf->st_mode = __wdtoxmode(findbuf.dwFileAttributes, name);
    buf->st_nlink = 1;

#ifdef _USE_INT64
    buf->st_size = ((__int64)(findbuf.nFileSizeHigh)) * (0x100000000i64) +
                    (__int64)(findbuf.nFileSizeLow);
#else  /* _USE_INT64 */
    buf->st_size = findbuf.nFileSizeLow;

    /* If the file size > 4GB, we get an overflow, so reset the file size
       and set the appropriate error */
    if (findbuf.nFileSizeHigh != 0)
    {
        errno = EOVERFLOW;
        buf->st_size = 0;
        retval = -1;
    }
#endif  /* _USE_INT64 */

    /* now set the common fields */

    buf->st_uid = buf->st_gid = buf->st_ino = 0;

    buf->st_rdev = buf->st_dev = (_dev_t)(drive - 1); /* A=0, B=1, etc. */

    return retval;
}


/*
 * IsRootUNCName - returns TRUE if the argument is a UNC name specifying
 *      a root share.  That is, if it is of the form \\server\share\.
 *      This routine will also return true if the argument is of the
 *      form \\server\share (no trailing slash) but Win32 currently
 *      does not like that form.
 *
 *      Forward slashes ('/') may be used instead of backslashes ('\').
 */

static int IsRootUNCName(const wchar_t *path)
{
    /*
     * If a root UNC name, path will start with 2 (but not 3) slashes
     */

    if ( ( wcslen ( path ) >= 5 ) /* minimum string is "//x/y" */
         && ISSLASH(path[0]) && ISSLASH(path[1])
         && !ISSLASH(path[2]))
    {
        const wchar_t * p = path + 2 ;

        /*
         * find the slash between the server name and share name
         */
        while ( * ++ p )
        {
            if ( ISSLASH(*p) )
            {
                break;
            }
        }

        if ( *p && p[1] )
        {
            /*
             * is there a further slash?
             */
            while ( * ++ p )
            {
                if ( ISSLASH(*p) )
                {
                    break;
                }
            }

            /*
             * just final slash (or no final slash)
             */
            if ( !*p || !p[1])
            {
                return 1;
            }
        }
    }

    return 0 ;
}

static wchar_t * __cdecl _wfullpath_helper(wchar_t * buf,const wchar_t *path,size_t sz, wchar_t ** pBuf)
{
    wchar_t * ret;
    errno_t save_errno = errno;

    errno = 0;
    ret = _wfullpath(buf, path, sz);
    if (ret)
    {
        errno = save_errno;
        return ret;
    }

    /* if _wfullpath fails because buf is too small, then we just call again _tfullpath and
     * have it allocate the appropriate buffer
     */
    if (errno != ERANGE)
    {
        /* _wfullpath is failing for another reason, just propagate the failure and keep the
         * failure code in errno
         */
        return NULL;
    }
    errno = save_errno;

    *pBuf = _wfullpath(NULL, path, 0);

    return *pBuf;
}

#endif  /* _UNICODE */
