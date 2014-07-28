/***
*lseek.c - change file position
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _lseek() - move the file pointer
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <mtdll.h>
#include <io.h>
#include <internal.h>
#include <stdlib.h>
#include <errno.h>
#include <msdos.h>
#include <stdio.h>

/***
*long _lseek(fh,pos,mthd) - move the file pointer
*
*Purpose:
*       Moves the file pointer associated with fh to a new position.
*       The new position is pos bytes (pos may be negative) away
*       from the origin specified by mthd.
*
*       If mthd == SEEK_SET, the origin in the beginning of file
*       If mthd == SEEK_CUR, the origin is the current file pointer position
*       If mthd == SEEK_END, the origin is the end of the file
*
*       Multi-thread:
*       _lseek()    = locks/unlocks the file
*       _lseek_nolock() = does NOT lock/unlock the file (it is assumed that
*                     the caller has the aquired the file lock,if needed).
*
*Entry:
*       int fh - file handle to move file pointer on
*       long pos - position to move to, relative to origin
*       int mthd - specifies the origin pos is relative to (see above)
*
*Exit:
*       returns the offset, in bytes, of the new position from the beginning
*       of the file.
*       returns -1L (and sets errno) if fails.
*       Note that seeking beyond the end of the file is not an error.
*       (although seeking before the beginning is.)
*
*Exceptions:
*
*******************************************************************************/

/* define locking/validating lseek */
long __cdecl _lseek (
        int fh,
        long pos,
        int mthd
        )
{
        int r = 0;

        /* validate fh */
        _CHECK_FH_CLEAR_OSSERR_RETURN( fh, EBADF, -1 );
        _VALIDATE_CLEAR_OSSERR_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(fh) & FOPEN), EBADF, -1);

        _lock_fh(fh);                   /* lock file handle */

        __try {
                if ( _osfile(fh) & FOPEN )
                        r = _lseek_nolock(fh, pos, mthd);   /* seek */
                else {
                        errno = EBADF;
                        _doserrno = 0;
                        r = -1;
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
                }
        }
        __finally {
                _unlock_fh(fh);         /* unlock file handle */
        }

        return r;
}

/* define core _lseek -- doesn't lock or validate fh */
long __cdecl _lseek_nolock (
        int fh,
        long pos,
        int mthd
        )
{
        LARGE_INTEGER  large_pos;        /* file position */
        LARGE_INTEGER  saved_pos;        /* original file position */
        LARGE_INTEGER  new_pos;          /* new file position */
        HANDLE osHandle;        /* o.s. handle value */

        /* tell o.s. to seek */

#if SEEK_SET != FILE_BEGIN || SEEK_CUR != FILE_CURRENT || SEEK_END != FILE_END
    #error Xenix and Win32 seek constants not compatible
#endif  /* SEEK_SET != FILE_BEGIN || SEEK_CUR != FILE_CURRENT || SEEK_END != FILE_END */
        if ((osHandle = (HANDLE)_get_osfhandle(fh)) == (HANDLE)-1)
        {
            errno = EBADF;
            _ASSERTE(("Invalid file descriptor",0));
            return -1;
        }

        large_pos.QuadPart = 0;

        /* Save the current file pointer */
        if (!SetFilePointerEx(osHandle, large_pos, &saved_pos, FILE_CURRENT)) {
            _dosmaperr(GetLastError());
            return -1;
        }

        /* Try to set the new file pointer */
        large_pos.QuadPart = pos;

        if (!SetFilePointerEx(osHandle, large_pos, &new_pos, mthd)) {
            _dosmaperr(GetLastError());
            return -1;
        }

        /* The call succeeded, but the new file pointer location is
           too large for the return type or a negative value.
           So, restore file pointer to saved location and return error.
         */
        if (new_pos.HighPart != 0) {
            /*  */
            SetFilePointerEx(osHandle, saved_pos, NULL, FILE_BEGIN);
            errno = EINVAL;
            return -1;
        }

        _osfile(fh) &= ~FEOFLAG;        /* clear the ctrl-z flag on the file */
        return new_pos.LowPart;         /* return */
}
