/***
*strftime.c - String Format Time
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <mtdll.h>
#include <time.h>
#include <locale.h>
#include <setlocal.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dbgint.h>
#include <malloc.h>
#include <errno.h>

extern "C" size_t __cdecl _Wcsftime_l (
        wchar_t *string,
        size_t maxsize,
        const wchar_t *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        );

/* Prototypes for local routines */
extern "C" size_t __cdecl _Strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg
        );

extern "C" size_t __cdecl _Strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        );

#define TIME_SEP        ':'

/*      get a copy of the current day names */
extern "C" char * __cdecl _Getdays_l (
        _locale_t plocinfo
        )
{
    const struct __lc_time_data *pt;
    size_t n, len = 0;
    char *p;
    _LocaleUpdate _loc_update(plocinfo);

    pt = __LC_TIME_CURR(_loc_update.GetLocaleT()->locinfo);

    for (n = 0; n < 7; ++n)
        len += strlen(pt->wday_abbr[n]) + strlen(pt->wday[n]) + 2;
    p = (char *)_malloc_crt(len + 1);

    if (p != 0) {
        char *s = p;

        for (n = 0; n < 7; ++n) {
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->wday_abbr[n]));
            s += strlen(s);
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->wday[n]));
            s += strlen(s);
        }
        *s++ = '\0';
    }

    return (p);
}
extern "C" char * __cdecl _Getdays (
        void
        )
{
    return _Getdays_l(NULL);
}

/*      get a copy of the current month names */
extern "C" char * __cdecl _Getmonths_l (
        _locale_t plocinfo
        )
{
    const struct __lc_time_data *pt;
    size_t n, len = 0;
    char *p;
    _LocaleUpdate _loc_update(plocinfo);

    pt = __LC_TIME_CURR(_loc_update.GetLocaleT()->locinfo);

    for (n = 0; n < 12; ++n)
        len += strlen(pt->month_abbr[n]) + strlen(pt->month[n]) + 2;
    p = (char *)_malloc_crt(len + 1);

    if (p != 0) {
        char *s = p;

        for (n = 0; n < 12; ++n) {
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->month_abbr[n]));
            s += strlen(s);
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->month[n]));
            s += strlen(s);
        }
        *s++ = '\0';
    }

    return (p);
}
extern "C" char * __cdecl _Getmonths (
        void
        )
{
    return _Getmonths_l(NULL);
}

/*      get a copy of the current time locale information */
extern "C" void * __cdecl _W_Gettnames_l (
        _locale_t plocinfo
        );
extern "C" void * __cdecl _Gettnames (
        void
        )
{
    return _W_Gettnames_l(NULL);
}


/***
*size_t strftime(string, maxsize, format, timeptr) - Format a time string
*
*Purpose:
*       Place characters into the user's output buffer expanding time
*       format directives as described in the user's control string.
*       Use the supplied 'tm' structure for time data when expanding
*       the format directives.
*       [ANSI]
*
*Entry:
*       char *string = pointer to output string
*       size_t maxsize = max length of string
*       const char *format = format control string
*       const struct tm *timeptr = pointer to tb data structure
*
*Exit:
*       !0 = If the total number of resulting characters including the
*       terminating null is not more than 'maxsize', then return the
*       number of chars placed in the 'string' array (not including the
*       null terminator).
*
*       0 = Otherwise, return 0 and the contents of the string are
*       indeterminate.
*
*Exceptions:
*
*******************************************************************************/

extern "C" size_t __cdecl _strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        _locale_t plocinfo
        )
{
        return (_Strftime_l(string, maxsize, format, timeptr, 0, plocinfo));
}
extern "C" size_t __cdecl strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr
        )
{
        return (_Strftime_l(string, maxsize, format, timeptr, 0, NULL));
}

/***
*size_t _Strftime(string, maxsize, format,
*       timeptr, lc_time) - Format a time string for a given locale
*
*Purpose:
*       Place characters into the user's output buffer expanding time
*       format directives as described in the user's control string.
*       Use the supplied 'tm' structure for time data when expanding
*       the format directives. use the locale information at lc_time.
*       [ANSI]
*
*Entry:
*       char *string = pointer to output string
*       size_t maxsize = max length of string
*       const char *format = format control string
*       const struct tm *timeptr = pointer to tb data structure
*               struct __lc_time_data *lc_time = pointer to locale-specific info
*                       (passed as void * to avoid type mismatch with C++)
*
*Exit:
*       !0 = If the total number of resulting characters including the
*       terminating null is not more than 'maxsize', then return the
*       number of chars placed in the 'string' array (not including the
*       null terminator).
*
*       0 = Otherwise, return 0 and the contents of the string are
*       indeterminate.
*
*Exceptions:
*
*******************************************************************************/

extern "C" size_t __cdecl _Strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg
        )
{
    return _Strftime_l(string, maxsize, format, timeptr,
                        lc_time_arg, NULL);
}

extern "C" size_t __cdecl _Strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        )
{
        size_t ret = 0;
        wchar_t *wformat = NULL;
        wchar_t *wstring = NULL;
        int cch_wformat;
        _LocaleUpdate _loc_update(plocinfo);

        _VALIDATE_RETURN( ( string != NULL ), EINVAL, 0)
        _VALIDATE_RETURN( ( maxsize != 0 ), EINVAL, 0)
        *string = '\0';

        _VALIDATE_RETURN( ( format != NULL ), EINVAL, 0)
        _VALIDATE_RETURN( ( timeptr != NULL ), EINVAL, 0)

        /* get the number of characters, including terminating null, needed for conversion */
        if  ( (cch_wformat = MultiByteToWideChar(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0 /* Use default flags */, format, -1, NULL, 0) ) == 0 )
        {
            _dosmaperr(GetLastError());
            goto error_cleanup;
        }

        /* allocate enough space for wide char format string */
        if ( (wformat = (wchar_t*)_malloc_crt( cch_wformat * sizeof(wchar_t) ) ) == NULL )
        {
            /* malloc should set the errno, if any */
            goto error_cleanup;
        }

        /* Copy format to a wide-char string */
        if ( MultiByteToWideChar(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0 /* Use default flags */, format, -1, wformat, cch_wformat) == 0 )
        {
            _dosmaperr(GetLastError());
            goto error_cleanup;
        }

        /* Allocate a new wide-char output string with the same size as the char* string one passed in as argument */
        if ((wstring = (wchar_t*)_malloc_crt(maxsize * sizeof(wchar_t))) == 0)
        {
            goto error_cleanup;
        }

        if (0 != (ret = _Wcsftime_l(wstring, maxsize, wformat, timeptr, lc_time_arg, plocinfo)))
        {
            /* Copy output from wide char string */
            if (!WideCharToMultiByte(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0, wstring, -1, string, (int)maxsize, NULL, NULL))
            {
                /* If conversion doesn't succeed */
                _dosmaperr(GetLastError());
                ret = 0; /* reset return status to indicate error */
            }
        }

error_cleanup:
        /* Cleanup */
        _free_crt(wstring);
        _free_crt(wformat);

        return ret;
}
