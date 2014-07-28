/* _Dtest function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN
 #if !defined(MRTDLL)
_C_LIB_DECL
 #endif /* defined(MRTDLL) */

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _Dtest(double *px)
	{	/* categorize *px */
	_Dval *ps = (_Dval *)(char *)px;

	if ((ps->_Sh[_D0] & _DMASK) == _DMAX << _DOFF)
		return ((short)((ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] != 0
			|| ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0 ? _NANCODE : _INFCODE));
	else if ((ps->_Sh[_D0] & ~_DSIGN) != 0 || ps->_Sh[_D1] != 0
		|| ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0)
		return ((ps->_Sh[_D0] & _DMASK) == 0 ? _DENORM : _FINITE);
	else
		return (0);
	}

_CRTIMP2_PURE unsigned short *__CLRCALL_PURE_OR_CDECL _Plsw(double *px)
	{	/* get pointer to lsw */
	return (&((_Dval *)(char *)px)->_Sh[_Dg]);
	}

_CRTIMP2_PURE unsigned short *__CLRCALL_PURE_OR_CDECL _Pmsw(double *px)
	{	/* get pointer to msw */
	return (&((_Dval *)(char *)px)->_Sh[_D0]);
	}
 #if !defined(MRTDLL)
_END_C_LIB_DECL
 #endif /* !defined(MRTDLL) */
_C_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
