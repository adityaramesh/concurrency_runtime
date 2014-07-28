/* wrapper to suppress warnings in windows.h
 */
#ifndef _WRAPWIN_H
#define _WRAPWIN_H

#include <string.h>

#ifdef _WIN32_WCE
 #include <stdlib.h>
 #include <time.h>
#endif /* _WIN32_WCE */

#pragma warning(disable: 4005 4052 4115 4201 4214)

#include <windows.h>
#endif /* _WRAPWIN_H */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
