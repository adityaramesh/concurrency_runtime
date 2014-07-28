// xrngdev: random device for TR1 random number generators
#define _CRT_RAND_S	// for rand_s
#include <stdexcept>	// for out_of_range

//  #include <random>
_STD_BEGIN
_CRTIMP2_PURE unsigned int __CLRCALL_PURE_OR_CDECL _Random_device();

_CRTIMP2_PURE unsigned int __CLRCALL_PURE_OR_CDECL _Random_device()
	{	// return a random value
	unsigned int ans;
	if (_CSTD rand_s(&ans))
		_Xout_of_range("invalid random_device value");
	return (ans);
	}

_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
