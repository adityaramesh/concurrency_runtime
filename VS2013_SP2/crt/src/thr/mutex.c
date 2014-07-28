/* mutex.c -- mutex functions */
/* NB: compile as C++ if _WIN32_CLIB && !defined(_M_CEE) */
#include <thr/threads.h>
#include <thr/xtimec.h>
#include <stdlib.h>

  #include <concrt.h>
  #include <dbgint.h>
  #include <ppl.h>
#include <wrapwin.h>

struct _Mtx_internal_imp_t
	{	/* Win32 mutex */
	int type;
	Concurrency::critical_section cs;
	long thread_id;
	int count;
	};

int _Mtx_init(_Mtx_t *mtx, int type)
	{	/* initialize mutex */
	_Mtx_t mutex;
	*mtx = 0;

	if ((mutex = (mtx_t)_calloc_crt(1, sizeof (struct _Mtx_internal_imp_t)))
		== 0)
		return (_Thrd_nomem);	/* report alloc failed */

	new (&mutex->cs) Concurrency::critical_section();
	mutex->thread_id = -1;
	mutex->type = type;
	*mtx = mutex;
	return (_Thrd_success);
	}

void _Mtx_destroy(_Mtx_t *mtx)
	{	/* destroy mutex */
	if (mtx && *mtx)
		{	/* something to do, do it */
		_THREAD_ASSERT((*mtx)->count == 0, "mutex destroyed while busy");
		(*mtx)->cs.~critical_section();
		_free_crt(*mtx);

		}
	}

static int mtx_do_lock(_Mtx_t *mtx, const xtime *target)
	{	/* lock mutex */
	if (((*mtx)->type & ~_Mtx_recursive) == _Mtx_plain)
		{	/* set the lock */
		if ((*mtx)->thread_id != GetCurrentThreadId())
			{	/* not current thread, do lock */
			(*mtx)->cs.lock();
			(*mtx)->thread_id = GetCurrentThreadId();
			}
		++(*mtx)->count;

		return (_Thrd_success);
		}
	else
		{	/* handle timed or recursive mutex */
		int res = WAIT_TIMEOUT;
		if (target == 0)
			{	/* no target --> plain wait (i.e. infinite timeout) */
			if ((*mtx)->thread_id != GetCurrentThreadId())
				(*mtx)->cs.lock();
			res = WAIT_OBJECT_0;

			}
		else if (target->sec < 0 || target->sec == 0 && target->nsec <= 0)
			{	/* target time <= 0 --> plain trylock or timed wait for */
				/* time that has passed; try to lock with 0 timeout */
				if ((*mtx)->thread_id != GetCurrentThreadId())
					{	/* not this thread, lock it */
					if ((*mtx)->cs.try_lock())
						res = WAIT_OBJECT_0;
					else
						res = WAIT_TIMEOUT;
					}
				else
					res = WAIT_OBJECT_0;

			}
		else
			{	/* check timeout */
			xtime now;
			xtime_get(&now, TIME_UTC);
			while (now.sec < target->sec
				|| now.sec == target->sec && now.nsec < target->nsec)
				{	/* time has not expired */
				if ((*mtx)->thread_id == GetCurrentThreadId()
					|| (*mtx)->cs.try_lock_for(
						_Xtime_diff_to_millis2(target, &now)))
					{	/* stop waiting */
					res = WAIT_OBJECT_0;
					break;
					}
				else
					res = WAIT_TIMEOUT;

				xtime_get(&now, TIME_UTC);
				}
			}
		if (res != WAIT_OBJECT_0 && res != WAIT_ABANDONED)
			;

		else if (1 < ++(*mtx)->count)
			{	/* check count */
			if (((*mtx)->type & _Mtx_recursive) != _Mtx_recursive)
				{	/* not recursive, fixup count */
				--(*mtx)->count;
				res = WAIT_TIMEOUT;
				}
			}
		else
			(*mtx)->thread_id = GetCurrentThreadId();

		switch (res)
			{
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED:
			return (_Thrd_success);

		case WAIT_TIMEOUT:
			if (target == 0 || (target->sec == 0 && target->nsec == 0))
				return (_Thrd_busy);
			else
				return (_Thrd_timedout);

		default:
			return (_Thrd_error);
			}
		}
	}

int _Mtx_unlock(_Mtx_t *mtx)
	{	/* unlock mutex */
	_THREAD_ASSERT(1 <= (*mtx)->count
		&& (*mtx)->thread_id == GetCurrentThreadId(),
		"unlock of unowned mutex");

	if (--(*mtx)->count == 0)
		{	/* leave critical section */
		(*mtx)->thread_id = -1;
		(*mtx)->cs.unlock();
		}
	return (_Thrd_success);

	}

int _Mtx_lock(_Mtx_t *mtx)
	{	/* lock mutex */
	return (mtx_do_lock(mtx, 0));
	}

int _Mtx_trylock(_Mtx_t *mtx)
	{	/* attempt to lock try_mutex */
	xtime xt;
	_THREAD_ASSERT(((*mtx)->type & (_Mtx_try | _Mtx_timed)) != 0,
		"trylock not supported by mutex");
	xt.sec = xt.nsec = 0;
	return (mtx_do_lock(mtx, &xt));
	}

int _Mtx_timedlock(_Mtx_t *mtx, const xtime *xt)
	{	/* attempt to lock timed mutex */
	int res;

	_THREAD_ASSERT(((*mtx)->type & _Mtx_timed) != 0,
		"timedlock not supported by mutex");
	res = mtx_do_lock(mtx, xt);
	return (res == _Thrd_busy ? _Thrd_timedout : res);
	}

int _Mtx_current_owns(_Mtx_t *mtx)
	{	/* test if current thread owns mutex */
	return ((*mtx)->count != 0
		&& (*mtx)->thread_id == GetCurrentThreadId());
	}

void * _Mtx_getconcrtcs(_Mtx_t *mtx)
	{	/* get cs for Concrt */
	return (&(*mtx)->cs);
	}

void _Mtx_clear_owner(_Mtx_t *mtx)
	{	/* set owner to nobody */
	(*mtx)->thread_id = -1;
	--(*mtx)->count;
	}

void _Mtx_reset_owner(_Mtx_t *mtx)
	{	/* set owner to current thread */
	(*mtx)->thread_id = GetCurrentThreadId();
	++(*mtx)->count;
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
