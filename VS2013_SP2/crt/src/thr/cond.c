/* cond.c -- condition variable functions */
/* NB: compile as C++ if _WIN32_CLIB && !defined(_M_CEE) */
#include <thr/xthreads.h>
#include <thr/xtimec.h>

#include <stdlib.h>
#include <limits.h>

#include <dbgint.h>
#include <concrt.h>

#include <wrapwin.h>

struct _Cnd_internal_imp_t
	{	/* condition variable implementation for Win32 */
	Concurrency::details::_Condition_variable cv;

	};

int _Cnd_init(_Cnd_t *pcond)
	{	/* initialize */
	_Cnd_t cond;
	*pcond = 0;

	if ((cond = (_Cnd_t)_calloc_crt(1,
		sizeof(struct _Cnd_internal_imp_t))) == 0)
		return (_Thrd_nomem);	/* report alloc failed */
	else
		{	/* report success */
		new (&cond->cv) Concurrency::details::_Condition_variable();
		*pcond = cond;
		return (_Thrd_success);
		}
	}

void _Cnd_destroy(_Cnd_t *cond)
	{	/* clean up */
	if (cond && *cond)
		{	/* something to do, do it */
		(*cond)->cv.~_Condition_variable();
		_free_crt(*cond);
		}
	}

static int do_wait(_Cnd_t *cond, _Mtx_t *mtx, const xtime *target)
	{	/* wait for signal or timeout */
	int res = _Thrd_success;
	Concurrency::critical_section *cs =
		(Concurrency::critical_section *) _Mtx_getconcrtcs(mtx);
	if (target == 0)
		{	/* no target time specified, wait on mutex */
		_Mtx_clear_owner(mtx);
		(*cond)->cv.wait(*cs);
		_Mtx_reset_owner(mtx);
		}
	else
		{	/* target time specified, wait for it */
		xtime now;
		xtime_get(&now, TIME_UTC);
		_Mtx_clear_owner(mtx);
		if (!(*cond)->cv.wait_for(*cs, _Xtime_diff_to_millis2(target, &now)))
			res = _Thrd_timedout;
		_Mtx_reset_owner(mtx);
		}
	return (res);

	}

static int do_signal(_Cnd_t *cond, int all)
	{	/* release threads */
	if (all)
		(*cond)->cv.notify_all();
	else
		(*cond)->cv.notify_one();
	return (_Thrd_success);

	}

int _Cnd_wait(_Cnd_t *cond, _Mtx_t *mtx)
	{	/* wait until signaled */
	return (do_wait(cond, mtx, 0));
	}

int _Cnd_timedwait(_Cnd_t *cond, _Mtx_t *mtx, const xtime *xt)
	{	/* wait until signaled or timeout */
	return (do_wait(cond, mtx, xt));
	}

int _Cnd_signal(_Cnd_t *cond)
	{	/* release one waiting thread */
	return (do_signal(cond, 0));
	}

int _Cnd_broadcast(_Cnd_t *cond)
	{	/* release all waiting threads */
	return (do_signal(cond, 1));
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
