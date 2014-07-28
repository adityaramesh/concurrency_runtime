// future.cpp -- future static objects
#include <cstdlib>

 #include <future>
 #include <exception>
_STD_BEGIN

_CRTIMP2_PURE const char *__CLRCALL_PURE_OR_CDECL _Future_error_map(int _Errcode) _NOEXCEPT
	{	// convert to name of future error
	switch (static_cast<_Future_errc>(_Errcode))
		{	// switch on error code value
	case future_errc::broken_promise:
		return ("broken promise");

	case future_errc::future_already_retrieved:
		return ("future already retrieved");

	case future_errc::promise_already_satisfied:
		return ("promise already satisfied");

	case future_errc::no_state:
		return ("no state");

	default:
		return (0);
		}
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Throw_future_error(const error_code& _Code))
	{	// throw an exception
	_THROW_NCEE(future_error, _Code);
	}

_CRTIMP2_PURE _NO_RETURN(__CLRCALL_PURE_OR_CDECL _Rethrow_future_exception(
	_XSTD exception_ptr _Ptr))
	{	// rethrow an exception
	_XSTD rethrow_exception(_Ptr);
	}

_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
