/***
*dbgdel2.cpp - defines C++ scalar and array delete operators, debug versions
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines C++ scalar and array delete operators, debug versions.
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <dbgint.h>

/* These versions are declared in crtdbg.h.
 * Implementing there operators in the CRT libraries makes it easier to override them.
 */

void operator delete(void * pUserData, int, const char *, int)
{
    ::operator delete(pUserData);
}

void operator delete[](void * pUserData, int, const char *, int)
{
    ::operator delete[](pUserData);
}
