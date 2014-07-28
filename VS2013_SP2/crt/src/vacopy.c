/* _vacopy function */
#include <stdarg.h>
#include <string.h>

void _CRTIMP __cdecl _vacopy(va_list *pap, va_list ap)
{
    *pap = ap;
}

/*
* Copyright (c) 1992-2010 by P.J. Plauger.  ALL RIGHTS RESERVED.
* Consult your license regarding permissions and restrictions.
V5.30:0009 */
