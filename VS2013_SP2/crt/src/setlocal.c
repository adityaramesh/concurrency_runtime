/***
*setlocal.c - Contains the setlocale function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Contains the _wsetlocale() function.
*
*******************************************************************************/

#include <wchar.h>
#include <stdlib.h>
#include <setlocal.h>
#include <locale.h>
#include <dbgint.h>
#include <mtdll.h>
#include <internal.h>
#include <malloc.h>

char * __cdecl setlocale (
        int _category,
        const char *_locale
        )
{
        size_t size = 0;
        wchar_t *inwlocale = NULL;
        wchar_t *outwlocale;
        pthreadlocinfo ptloci;
        int *refcount = NULL;
        char *outlocale = NULL;
        _locale_tstruct locale;
        _ptiddata ptd;

        /* convert ASCII string into WCS string */
        if (_locale)
        {
            _ERRCHECK_EINVAL_ERANGE(mbstowcs_s(&size, NULL, 0, _locale, INT_MAX));
            if ((inwlocale = (wchar_t *)_calloc_crt(size, sizeof(wchar_t))) == NULL)
                return NULL;

            if (_ERRCHECK_EINVAL_ERANGE(mbstowcs_s(NULL, inwlocale, size, _locale, _TRUNCATE)) != 0)
            {
                _free_crt (inwlocale);
                return NULL;
            }
        }

        /* set the locale and get WCS return string */

        outwlocale = _wsetlocale(_category, inwlocale);
        _free_crt (inwlocale);

        if (NULL == outwlocale)
            return NULL;

        // We now have a locale string, but the global locale can be changed by
        // another thread. If we allow this thread's locale to be updated before we're done
        // with this string, it might be freed from under us.
        // Call versions of the wide-to-MB-char conversions that do not update the current thread's
        // locale.

        ptd = _getptd();
        locale.locinfo = ptd->ptlocinfo;
        locale.mbcinfo = ptd->ptmbcinfo;

        /* get space for WCS return value, first call only */

        size = 0;
        if (_ERRCHECK_EINVAL_ERANGE(_wcstombs_s_l(&size, NULL, 0, outwlocale, 0, &locale)) != 0)
            return NULL;

        refcount = (int *)_malloc_crt(size + sizeof(int));
        if (!refcount)
            return NULL;

        outlocale = (char*)&refcount[1];

        /* convert return value to ASCII */
        if ( _ERRCHECK_EINVAL_ERANGE(_wcstombs_s_l(NULL, outlocale, size, outwlocale, _TRUNCATE, &locale)) != 0)
        {
            _free_crt(refcount);
            return NULL;
        }

        ptloci = locale.locinfo;
        _mlock(_SETLOCALE_LOCK);
        __try {
            _ASSERTE(((ptloci->lc_category[_category].locale != NULL) && (ptloci->lc_category[_category].refcount != NULL)) ||
                     ((ptloci->lc_category[_category].locale == NULL) && (ptloci->lc_category[_category].refcount == NULL)));
            if (ptloci->lc_category[_category].refcount != NULL &&
                InterlockedDecrement(ptloci->lc_category[_category].refcount) == 0) {
                _free_crt(ptloci->lc_category[_category].refcount);
            }

            if (!(ptd->_ownlocale & _PER_THREAD_LOCALE_BIT) &&
                        !(__globallocalestatus & _GLOBAL_LOCALE_BIT)) {
                if (ptloci->lc_category[_category].refcount != NULL &&
                    InterlockedDecrement(ptloci->lc_category[_category].refcount) == 0) {
                    _free_crt(ptloci->lc_category[_category].refcount);
                }
            }

            /*
             * Note that we are using a risky trick here.  We are adding this
             * locale to an existing threadlocinfo struct, and thus starting
             * the locale's refcount with the same value as the whole struct.
             * That means all code which modifies both threadlocinfo::refcount
             * and threadlocinfo::lc_category[]::refcount in structs that are
             * potentially shared across threads must make those modifications
             * under _SETLOCALE_LOCK.  Otherwise, there's a race condition
             * for some other thread modifying threadlocinfo::refcount after
             * we load it but before we store it to refcount.
             */
            *refcount = ptloci->refcount;
            ptloci->lc_category[_category].refcount = refcount;
            ptloci->lc_category[_category].locale = outlocale;
        }
        __finally {
            _munlock(_SETLOCALE_LOCK);
        }

        return outlocale;
}
