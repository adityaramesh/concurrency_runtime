/* imaxabs function */
#include <inttypes.h>

intmax_t _CRTIMP __cdecl imaxabs(intmax_t i)
{ /* compute absolute value of intmax_t argument */
    return (i < 0 ? -i : i);
}

/*
 * Copyright (c) 1992-2010 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V5.30:0009 */
