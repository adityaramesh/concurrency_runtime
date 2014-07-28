// uncaught -- uncaught_exception for Microsoft

  #include <eh.h>
  #include <exception>
_STD_BEGIN

_CRTIMP2_PURE bool __CLRCALL_PURE_OR_CDECL uncaught_exception()
	{	// report if handling a throw
	return (__uncaught_exception());
	}
_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
