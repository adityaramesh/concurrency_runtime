/***
*ftell.c - get current file position
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines ftell() - find current current position of file pointer
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <errno.h>
#include <msdos.h>
#include <stddef.h>
#include <io.h>
#include <internal.h>
#include <mtdll.h>

/***
*long ftell(stream) - query stream file pointer
*
*Purpose:
*       Find out what stream's position is. coordinate with buffering; adjust
*       backward for read-ahead and forward for write-behind. This is NOT
*       equivalent to fseek(stream,0L,1), because fseek will remove an ungetc,
*       may flush buffers, etc.
*
*Entry:
*       FILE *stream - stream to query for position
*
*Exit:
*       return present file position if succeeds
*       returns -1L and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

long __cdecl ftell (
        FILE *stream
        )
{
    long retval = 0;

    _VALIDATE_RETURN( (stream != NULL), EINVAL, (-1L) );

    _lock_str(stream);

    __try {
        retval = _ftell_nolock (stream);
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}


/***
*_ftell_nolock() - Ftell() core routine (assumes stream is locked).
*
*Purpose:
*       Core ftell() routine; assumes caller has aquired stream lock).
*
*       [See ftell() above for more info.]
*
*Entry: [See ftell()]
*
*Exit:  [See ftell()]
*
*Exceptions:
*
*******************************************************************************/

long __cdecl _ftell_nolock (
        FILE *str
        )
{
        FILE *stream;
        unsigned int offset;
        long filepos;
        char *p;
        char *max;
        int fd;
        unsigned int rdcnt;
        char tmode;

        _VALIDATE_RETURN( (str != NULL), EINVAL, (-1L) );

        /* Init stream pointer and file descriptor */
        stream = str;
        fd = _fileno(stream);

        if (stream->_cnt < 0)
            stream->_cnt = 0;

        if ((filepos = _lseek(fd, 0L, SEEK_CUR)) < 0L)
            return(-1L);

        /* _lseek validates fd, so it's now ok to retrieve the textmode */
        tmode = _textmode(fd);

        if (!bigbuf(stream))            /* _IONBF or no buffering designated */
            return(filepos - stream->_cnt);

        offset = (unsigned)(stream->_ptr - stream->_base);

        if (stream->_flag & (_IOWRT|_IOREAD)) {
                if (tmode == __IOINFO_TM_UTF8 && _utf8translations(fd))
                {
                    size_t curpos = (size_t)(stream->_ptr - stream->_base) / sizeof(wchar_t);

                    if (stream->_cnt == 0)
                        return filepos;
                    else {
                        DWORD bytes_read;
                        char buf[_INTERNAL_BUFSIZ];

                        __int64 basepos = _lseeki64(fd, _startpos(fd), SEEK_SET);

                        if (basepos != _startpos(fd))
                            return (-1);
                        if ( !ReadFile( (HANDLE)_osfhnd(fd), buf, _INTERNAL_BUFSIZ, &bytes_read, NULL ) )
                            return (-1);
                        if (_lseek(fd, filepos, SEEK_SET) < 0)
                            return -1;
                        if (curpos > bytes_read)
                            return (-1);

                        p = buf;
                        while (curpos-- && (p < buf + bytes_read) )
                        {
                            if (*p == CR) {
                                /* *p is CR, so must check next char for LF */
                                if (p < (char *)buf + bytes_read - 1) {
                                    if (*(p+1) == LF) {
                                        p ++;
                                    }
                                }
                            }
                            else {
                                p += _utf8_no_of_trailbytes(*p);
                            }
                            p++;
                        }
                        return (long) (basepos + (size_t)(p - buf));
                    }
                }
            if (_osfile(fd) & FTEXT)
                for (p = stream->_base; p < stream->_ptr; p++)
                    if (*p == '\n')  /* adjust for '\r' */
                        offset++;
        }
        else if (!(stream->_flag & _IORW)) {
            errno=EINVAL;
            return(-1L);
        }

        if (filepos == 0L)
            return((long)offset);

        if (stream->_flag & _IOREAD)    /* go to preceding sector */

            if (stream->_cnt == 0)  /* filepos holds correct location */
                offset = 0;

            else {

                /* Subtract out the number of unread bytes left in the buffer.
                   [We can't simply use _iob[]._bufsiz because the last read
                   may have hit EOF and, thus, the buffer was not completely
                   filled.] */

                rdcnt = stream->_cnt + (unsigned)(stream->_ptr - stream->_base);

                /* If text mode, adjust for the cr/lf substitution. If binary
                   mode, we're outta here. */
                if (_osfile(fd) & FTEXT) {
                    /* (1) If we're not at eof, simply copy _bufsiz onto rdcnt
                       to get the # of untranslated chars read. (2) If we're at
                       eof, we must look through the buffer expanding the '\n'
                       chars one at a time. */

                    /* [NOTE: Performance issue -- it is faster to do the two
                       _lseek() calls than to blindly go through and expand the
                       '\n' chars regardless of whether we're at eof or not.] */

                    if (_lseek(fd, 0L, SEEK_END) == filepos) {

                        max = stream->_base + rdcnt;
                        for (p = stream->_base; p < max; p++)
                            if (*p == '\n')
                                /* adjust for '\r' */
                                rdcnt++;

                        /* If last byte was ^Z, the lowio read didn't tell us
                           about it. Check flag and bump count, if necessary. */

                        if (stream->_flag & _IOCTRLZ)
                            ++rdcnt;
                    }

                    else {

                        if (_lseek(fd, filepos, SEEK_SET) < 0)
                            return (-1);

                        /* We want to set rdcnt to the number of bytes
                           originally read into the stream buffer (before
                           crlf->lf translation). In most cases, this will
                           just be _bufsiz. However, the buffer size may have
                           been changed, due to fseek optimization, at the
                           END of the last _filbuf call. */

                        if ( (rdcnt <= _SMALL_BUFSIZ) &&
                             (stream->_flag & _IOMYBUF) &&
                             !(stream->_flag & _IOSETVBUF) )
                        {
                            /* The translated contents of the buffer is small
                               and we are not at eof. The buffer size must have
                               been set to _SMALL_BUFSIZ during the last
                               _filbuf call. */

                            rdcnt = _SMALL_BUFSIZ;
                        }
                        else
                            rdcnt = stream->_bufsiz;

                        /* If first byte in untranslated buffer was a '\n',
                           assume it was preceeded by a '\r' which was
                           discarded by the previous read operation and count
                           the '\n'. */
                        if  (_osfile(fd) & FCRLF)
                            ++rdcnt;
                    }

                    if (tmode == __IOINFO_TM_UTF8)
                        rdcnt /= sizeof(wchar_t);
                } /* end if FTEXT */

                filepos -= (long)rdcnt;

            } /* end else stream->_cnt != 0 */

            if (tmode == __IOINFO_TM_UTF8)
                offset /= sizeof(wchar_t);
            return(filepos + (long)offset);
}
