/* xtime.c -- xtime functions */
#include <thr/xtimec.h>
#include <time.h>
#include <stdlib.h>

#define NSEC_PER_SEC 1000000000L
#define NSEC_PER_MSEC 1000000L
#define NSEC_PER_USEC 1000L
#define MSEC_PER_SEC 1000

static void xtime_normalize(xtime *xt)
	{	/* adjust so that 0 <= nsec < 1 000 000 000 */
	while (xt->nsec < 0)
		{	/* normalize target time */
		xt->sec -= 1;
		xt->nsec += NSEC_PER_SEC;
		}
	while (NSEC_PER_SEC <= xt->nsec)
		{	/* normalize target time */
		xt->sec += 1;
		xt->nsec -= NSEC_PER_SEC;
		}
	}

static xtime xtime_diff(const xtime *xt, const xtime *now)
	{	/* return xtime object holding difference between xt and now,
			treating negative difference as 0 */
	xtime diff = *xt;
	xtime_normalize(&diff);
	if (diff.nsec < now->nsec)
		{	// avoid underflow
		diff.sec -= now->sec + 1;
		diff.nsec += NSEC_PER_SEC - now->nsec;
		}
	else
		{	// no underflow
		diff.sec -= now->sec;
		diff.nsec -= now->nsec;
		}
	if (diff.sec < 0 || (diff.sec == 0 && diff.nsec <= 0))
		{	/* time is zero */
		diff.sec = 0;
		diff.nsec = 0;
		}
	return (diff);
	}

#include <wrapwin.h>

  #define EPOCH	0x19DB1DED53E8000i64

  #define NSEC100_PER_SEC	(NSEC_PER_SEC / 100)
  #define NSEC100_PER_MSEC	(NSEC_PER_MSEC / 100)

_LONGLONG _Xtime_get_ticks()
	{	/* get system time in 100-nanosecond intervals since the epoch */
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ((((_LONGLONG)ft.dwHighDateTime) << 32)
		+ (_LONGLONG)ft.dwLowDateTime - EPOCH);
	}

static void sys_get_time(xtime *xt)
	{	/* get system time with nanosecond resolution */
	_ULONGLONG now = _Xtime_get_ticks();
	xt->sec = (time_t)(now / NSEC100_PER_SEC);
	xt->nsec = (long)(now % NSEC100_PER_SEC) * 100;
	}

long _Xtime_diff_to_millis2(const xtime *xt1, const xtime *xt2)
	{	/* convert time to milliseconds */
	xtime diff = xtime_diff(xt1, xt2);
	return ((long)(diff.sec * MSEC_PER_SEC
		+ (diff.nsec + NSEC_PER_MSEC - 1) / NSEC_PER_MSEC));
	}

long _Xtime_diff_to_millis(const xtime *xt)
	{	/* convert time to milliseconds */
	xtime now;
	xtime_get(&now, TIME_UTC);
	return (_Xtime_diff_to_millis2(xt, &now));
	}

int xtime_get(xtime *xt, int type)
	{	/* get current time */
	if (type != TIME_UTC || xt == 0)
		type = 0;
	else
		sys_get_time(xt);
	return (type);
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
