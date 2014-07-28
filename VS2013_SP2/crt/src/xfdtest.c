/* _FDtest function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN
 #if !defined(MRTDLL)
_C_LIB_DECL
 #endif /* defined(MRTDLL) */

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDtest(float *px)
	{	/* categorize *px */
	_Fval *ps = (_Fval *)(char *)px;

	if ((ps->_Sh[_F0] & _FMASK) == _FMAX << _FOFF)
		return ((short)((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0
			? _NANCODE : _INFCODE));
	else if ((ps->_Sh[_F0] & ~_FSIGN) != 0 || ps->_Sh[_F1] != 0)
		return ((ps->_Sh[_F0] & _FMASK) == 0 ? _DENORM : _FINITE);
	else
		return (0);
	}

_CRTIMP2_PURE unsigned short *__CLRCALL_PURE_OR_CDECL _FPlsw(float *px)
	{	/* get pointer to lsw */
	return (&((_Fval *)(char *)px)->_Sh[_Fg]);
	}

_CRTIMP2_PURE unsigned short *__CLRCALL_PURE_OR_CDECL _FPmsw(float *px)
	{	/* get pointer to msw */
	return (&((_Fval *)(char *)px)->_Sh[_F0]);
	}
 #if !defined(MRTDLL)
_END_C_LIB_DECL
 #endif /* !defined(MRTDLL) */
_C_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
