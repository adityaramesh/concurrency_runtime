// threadcall.cpp -- Dinkum thread support functions
#include <thr/xthread>

 #if _HAS_EXCEPTIONS
#include <stdexcept>
#include <string>

 #else /* _HAS_EXCEPTIONS */
#include <cstdio>
#include <cstdlib>
 #endif /* _HAS_EXCEPTIONS */

#include <cstdint>

_STD_BEGIN


typedef unsigned int _Call_func_ret;
#define _CALL_FUNC_MODIFIER	_STDCALL

_EXTERN_C
static _Call_func_ret _CALL_FUNC_MODIFIER _Call_func(void *_Data)
	{	// entry point for new thread
    _Call_func_ret _Res = 0;
 #if _HAS_EXCEPTIONS
	try {	// don't let exceptions escape
		_Res = (_Call_func_ret)(_STD intptr_t)
			static_cast<_Pad *>(_Data)->_Go();
		}
	catch (...)
		{	// uncaught exception in thread
		int zero = 0;
		if (zero == 0)
 #if 1300 <= _MSC_VER
			terminate();	// to quiet diagnostics
 #else /* 1300 <= _MSC_VER */
			_XSTD terminate();	// to quiet diagnostics
 #endif /* 1300 <= _MSC_VER */
		}

 #else /* _HAS_EXCEPTIONS */
	_Res = (_Call_func_ret)static_cast<_Pad *>(_Data)->_Go();
 #endif /* _HAS_EXCEPTIONS */

	_Cnd_do_broadcast_at_thread_exit();
	return (_Res);
	}
_END_EXTERN_C

_Pad::_Pad()
	{	// initialize handshake
	_Cnd_initX(&_Cond);
	_Auto_cnd _Cnd_cleaner(&_Cond);
	_Mtx_initX(&_Mtx, _Mtx_plain);
	_Auto_mtx _Mtx_cleaner(&_Mtx);
	_Started = false;
	_Mtx_lockX(&_Mtx);
	_Mtx_cleaner._Release();
	_Cnd_cleaner._Release();
	}

_Pad::~_Pad() _NOEXCEPT
	{	// clean up handshake
	_Auto_cnd _Cnd_cleaner(&_Cond);
	_Auto_mtx _Mtx_cleaner(&_Mtx);
	_Mtx_unlockX(&_Mtx);
	}

void _Pad::_Launch(_Thrd_t *_Thr)
	{	// launch a thread
	_Thrd_startX(_Thr, _Call_func, this);
	while (!_Started)
		_Cnd_waitX(&_Cond, &_Mtx);
	}

void _Pad::_Release()
	{	// notify caller that it's okay to continue
	_Mtx_lockX(&_Mtx);
	_Started = true;
	_Cnd_signalX(&_Cond);
	_Mtx_unlockX(&_Mtx);
	}
_STD_END

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
