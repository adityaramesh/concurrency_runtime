/* tss.c -- thread specific storage support */
#include <thr/xthrcommon.h>
#include <thr/xthreads.h>

#include <wrapwin.h>

#include <awint.h>

  #define OUT_OF_INDEXES	FLS_OUT_OF_INDEXES
  #define TSS_ALLOC	__crtFlsAlloc(NULL)
  #define TSS_FREE	__crtFlsFree
  #define TSS_GET	__crtFlsGetValue
  #define TSS_SET	__crtFlsSetValue

typedef DWORD sys_key_t;

static int sys_tss_create(sys_key_t *key)
	{	/* create TSS */
	return ((*key = TSS_ALLOC) == OUT_OF_INDEXES
		? _Thrd_error : _Thrd_success);
	}

static int sys_tss_destroy(sys_key_t key)
	{	/* destroy TSS */
	return (TSS_FREE(key) == 0
		? _Thrd_error : _Thrd_success);
	}

static void **sys_tss_get(sys_key_t key)
	{	/* get TSS pointer */
	return ((void **)TSS_GET(key));
	}

static int sys_tss_set(sys_key_t key, const void *value)
	{	/* set TSS value */
	return (TSS_SET(key, (void *)value) == 0
		? _Thrd_error : _Thrd_success);
	}

 #include <stddef.h>
 #include <stdlib.h>

#define TSS_MAX_SLOTS	1000

typedef struct
	{	/* storage control */
	char inuse;
	_Tss_dtor_t dtor;
	} tss_ctrl_t;

static tss_ctrl_t tss_ctrl[TSS_MAX_SLOTS];

static _Once_flag tss_o = _ONCE_FLAG_INIT;
static int nextkey;
static _Mtx_t mtx;

static sys_key_t data_k;
static int initialized = 0;
static _Tss_t min_key = 0;

static void cleanup(void)
	{	/* tidy thread storage */
	_Tss_destroy();
	_Mtx_destroy(&mtx);
	if (initialized)
		sys_tss_destroy(data_k);
	}

static void init(void)
	{	/* initialize thread storage */
	if (sys_tss_create(&data_k) != _Thrd_success
		|| _Mtx_init(&mtx, _Mtx_plain) != _Thrd_success
		|| atexit(&cleanup) != 0)
		abort();
	initialized = 1;
	}

static void **getTssData(_Tss_t key)
	{	/* return pointer to thread storage for key */
	void **data = 0;
	if (key < min_key || TSS_MAX_SLOTS <= key)
		;
	else if ((data = sys_tss_get(data_k)) != 0)
		;
	else if ((data = (void **)calloc(TSS_MAX_SLOTS, sizeof (void *))) == 0
		|| sys_tss_set(data_k, data) != _Thrd_success)
		free(data), data = 0;
	return (data);
	}

static int alloc_tss(_Tss_t *key, _Tss_dtor_t dtor)
	{	/* allocate thread-local storage slot */
	int res = _Thrd_error;
	if (_Mtx_lock(&mtx) != _Thrd_success)
		return (res);
	while (nextkey < TSS_MAX_SLOTS && tss_ctrl[nextkey].inuse)
		++nextkey;
	if (nextkey != TSS_MAX_SLOTS)
		{	/* key slot available, allocate it */
		*key = nextkey++;
		tss_ctrl[*key].inuse = 1;
		tss_ctrl[*key].dtor = dtor;
		res = _Thrd_success;
		}
	if (_Mtx_unlock(&mtx) != _Thrd_success)
		res = _Thrd_error;
	return (res);
	}

void _Tss_destroy(void)
	{	/* destroy thread storage */
	int j = 0;
	int rpt = 1;
	void **data = sys_tss_get(data_k);
	if (data == 0)
		return;
	while (rpt && j++ < _TSS_DTOR_ITERATIONS)
		{	/* try several times, just in case */
		int i;
		rpt = 0;
		for (i = 0; i < TSS_MAX_SLOTS; ++i)
			if (tss_ctrl[i].inuse && tss_ctrl[i].dtor
				&& (data[i]) != 0)
				{	/* destroy a datum */
				void *tmp = data[i];
				data[i] = 0;
				tss_ctrl[i].dtor(tmp);
				rpt = 1;
				}
		}
	}

int _Tss_create(_Tss_t *key, _Tss_dtor_t dtor)
	{	/* allocate thread-local storage */
	_Call_once(&tss_o, init);
	return (alloc_tss(key, dtor));
	}

int _Tss_delete(_Tss_t key)
	{	/* free thread-local storage */
	if (key < min_key || TSS_MAX_SLOTS <= key)
		return (_Thrd_error);
	if (_Mtx_lock(&mtx) != _Thrd_success)
		return (_Thrd_error);
	tss_ctrl[key].inuse = 0;
	if (key < nextkey)
		nextkey = key;
	if (_Mtx_unlock(&mtx) != _Thrd_success)
		return (_Thrd_error);
	return (_Thrd_success);
	}

int _Tss_set(_Tss_t key, void *value)
	{	/* store value in thread-local storage */
	void **data = getTssData(key);
	if (data != 0)
		{	/* store value */
		data[key] = value;
		return (_Thrd_success);
		}
	return (_Thrd_error);
	}

void *_Tss_get(_Tss_t key)
	{	/* get value from thread-local storage */
	void **data = getTssData(key);
	return (data != 0 ? data[key] : 0);
	}

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
