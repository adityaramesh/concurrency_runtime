/* cthread.c -- thread functions */
#include <thr/xthreads.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
	{	/* bind function pointer and data to pass to thread entry point */
	_Thrd_start_t func;
	void *data;
	_Cnd_t *cond;
	_Mtx_t *mtx;
	int *started;
	} binder;

#include <wrapwin.h>

#include <awint.h>
#include <process.h>

static unsigned int _STDCALL run(void *d)
	{	/* call thread function */
	unsigned res;
	binder b = *(binder*)d;
	_Mtx_lock(b.mtx);
	*b.started = 1;
	_Cnd_signal(b.cond);
	_Mtx_unlock(b.mtx);
	res = (b.func)(b.data);
	_Tss_destroy();
	_Cnd_do_broadcast_at_thread_exit();
	return (res);
	}

void _Thrd_exit(int res)
	{	/* terminate execution of calling thread */
	_Tss_destroy();
 #if !defined(_CRT_APP) || defined(_KERNELX)
	_endthreadex(res);
 #endif /* !defined(_CRT_APP) || defined(_KERNELX) */
	}

int _Thrd_start(_Thrd_t *thr, _Thrd_callback_t func, void *b)
	{	/* start a thread */
 #if defined(_CRT_APP) && !defined(_KERNELX)
	return ((thr->_Hnd =
		(HANDLE)__crtCreateWinRTThread(func, b, &thr->_Id)) == 0
			? _Thrd_error : _Thrd_success);

 #else /* defined(_CRT_APP) && !defined(_KERNELX) */
	return ((thr->_Hnd =
		(HANDLE)_beginthreadex(0, 0, func, b, 0, &thr->_Id)) == 0
			? _Thrd_error : _Thrd_success);
 #endif /* defined(_CRT_APP) && !defined(_KERNELX) */
	}

int _Thrd_join(_Thrd_t thr, int *code)
	{	/* return exit code when thread terminates */
	unsigned long res;
 #if defined(_CRT_APP) && !defined(_KERNELX)
	if (__crtWaitForWinRTThreadExit(thr._Hnd, INFINITE) == WAIT_FAILED
		|| __crtGetExitCodeWinRTThread(thr._Hnd, &res) == 0)
		return (_Thrd_error);
	if (code)
		*code = (int)res;
	return (__crtCloseWinRTThreadHandle(thr._Hnd) == 0
		? _Thrd_error : _Thrd_success);

 #else /* defined(_CRT_APP) && !defined(_KERNELX) */
	if (WaitForSingleObject(thr._Hnd, INFINITE) == WAIT_FAILED
		|| GetExitCodeThread(thr._Hnd, &res) == 0)
		return (_Thrd_error);
	if (code)
		*code = (int)res;
	return (CloseHandle(thr._Hnd) == 0 ? _Thrd_error : _Thrd_success);
 #endif /* defined(_CRT_APP) && !defined(_KERNELX) */
	}

int _Thrd_detach(_Thrd_t thr)
	{	/* tell OS to release thread's resources when it terminates */
 #if defined(_CRT_APP) && !defined(_KERNELX)
	return (__crtCloseWinRTThreadHandle(thr._Hnd) == 0
		? _Thrd_error : _Thrd_success);
  #else /* defined(_CRT_APP) && !defined(_KERNELX) */
	return (CloseHandle(thr._Hnd) == 0 ? _Thrd_error : _Thrd_success);
  #endif /* defined(_CRT_APP) && !defined(_KERNELX) */
	}

void _Thrd_sleep(const xtime *xt)
	{	/* suspend thread until time xt */
	xtime now;
	xtime_get(&now, TIME_UTC);
	do  {	/* sleep and check time */
		__crtSleep(_Xtime_diff_to_millis2(xt, &now));
		xtime_get(&now, TIME_UTC);
		} while (now.sec < xt->sec
			|| now.sec == xt->sec && now.nsec < xt->nsec);
	}

void _Thrd_yield()
	{	/* surrender remainder of timeslice */
		__crtSleep(0);
	}

int _Thrd_equal(_Thrd_t thr0, _Thrd_t thr1)
	{	/* return 1 if thr0 and thr1 identify same thread */
	return (thr0._Id == thr1._Id);
	}

int _Thrd_lt(_Thrd_t thr0, _Thrd_t thr1)
	{	/* return 1 if thr0 < thr1 [must be a total ordering] */
	return (thr0._Id < thr1._Id);
	}

_Thrd_t _Thrd_current(void)
	{	/* return _Thrd_t identifying current thread */
	_Thrd_t thr;

#ifdef _WIN32_WCE
	thr._Hnd = (HANDLE)GetCurrentThreadId();
#elif defined(_CRT_APP) && !defined(_KERNELX)
	thr._Hnd = (HANDLE)__crtGetCurrentWinRTThread();
#else /* _WIN32_WCE */
	if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
		GetCurrentProcess(), &thr._Hnd,
		DUPLICATE_SAME_ACCESS, TRUE, 0))
		CloseHandle(thr._Hnd);
	else
		thr._Hnd = 0;
#endif /* _WIN32_WCE */

 #if defined(_CRT_APP) && !defined(_KERNELX)
	thr._Id = __crtGetCurrentWinRTThreadId();
 #else /* defined(_CRT_APP) && !defined(_KERNELX) */
	thr._Id = GetCurrentThreadId();
 #endif /* defined(_CRT_APP) && !defined(_KERNELX) */
	return (thr);
	}

void _Thrd_abort(const char *msg)
	{	/* abort on precondition failure */
	fputs(msg, stderr);
	fputc('\n', stderr);
	abort();
	}

int _Thrd_create(_Thrd_t *thr, _Thrd_start_t func, void *d)
	{	/* create thread */
	int res;
	binder b;
	int started = 0;
	_Cnd_t cond;
	_Mtx_t mtx;
	_Cnd_init(&cond);
	_Mtx_init(&mtx, _Mtx_plain);
	b.func = func;
	b.data = d;
	b.cond = &cond;
	b.mtx = &mtx;
	b.started = &started;
	_Mtx_lock(&mtx);
	if ((res = _Thrd_start(thr, run, &b)) == _Thrd_success)
	    {   /* wait for handshake */
    	while (!started)
    		_Cnd_wait(&cond, &mtx);
	    }
	_Mtx_unlock(&mtx);
	_Cnd_destroy(&cond);
	_Mtx_destroy(&mtx);
	return (res);
	}

/*
 * This file is derived from software bearing the following
 * restrictions:
 *
 * (c) Copyright William E. Kempf 2001
 *
 * Permission to use, copy, modify, distribute and sell this
 * software and its documentation for any purpose is hereby
 * granted without fee, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation. William E. Kempf makes no representations
 * about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
