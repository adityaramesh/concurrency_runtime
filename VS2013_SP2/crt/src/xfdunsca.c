/* _FDunscale function -- IEEE 754 version */
#include "xmath.h"
_C_STD_BEGIN
 #if !defined(MRTDLL)
_C_LIB_DECL
 #endif /* defined(MRTDLL) */

_CRTIMP2_PURE short __CLRCALL_PURE_OR_CDECL _FDunscale(short *pex, float *px)
	{	/* separate *px to 1/2 <= |frac| < 1 and 2^*pex */
	_Fval *ps = (_Fval *)(char *)px;
	short xchar = (ps->_Sh[_F0] & _FMASK) >> _FOFF;

	if (xchar == _FMAX)
		{	/* NaN or INF */
		*pex = 0;
		return ((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0
			? _NANCODE : _INFCODE);
		}
	else if (0 < xchar || (xchar = _FDnorm(ps)) <= 0)
		{	/* finite, reduce to [1/2, 1) */
		ps->_Sh[_F0] = (ps->_Sh[_F0] & ~_FMASK) | _FBIAS << _FOFF;
		*pex = xchar - _FBIAS;
		return (_FINITE);
		}
	else
		{	/* zero */
		*pex = 0;
		return (0);
		}
	}
 #if !defined(MRTDLL)
_END_C_LIB_DECL
 #endif /* !defined(MRTDLL) */
_C_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
