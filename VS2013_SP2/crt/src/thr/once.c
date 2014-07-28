/* once.c -- once functions */
#include <thr/xthreads.h>
#include <stdlib.h>

#include <wrapwin.h>

#include <awint.h>

static long once_state;
static CRITICAL_SECTION mtx;

static void cleanup(void)
	{	/* free resources at program termination */
	DeleteCriticalSection(&mtx);
	}

static void init_mutex(void)
	{	/* initialize once */
	long old;
	if (once_state == 2)
		;
	else if ((old = InterlockedExchange(&once_state, 1)) == 0)
		{	/* execute _Func, mark as executed */
		__crtInitializeCriticalSectionEx(&mtx, 4000, 0);
		atexit(cleanup);
		once_state = 2;
		}
	else if (old == 2)
		once_state = 2;
	else
		while (once_state != 2)
			__crtSleep(1);

	}

void _Call_once(_Once_flag *cntrl, void (*func)(void))
	{	/* execute func exactly one time */
	init_mutex();
	EnterCriticalSection(&mtx);
	if (*cntrl == _ONCE_FLAG_INIT)
		{	/* call func and mark as called */
		func();
		*cntrl = !_ONCE_FLAG_INIT;
		}
	LeaveCriticalSection(&mtx);
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
