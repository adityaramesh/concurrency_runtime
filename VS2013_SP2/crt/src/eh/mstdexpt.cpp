/***
*mstdexpt.cpp - defines pure msil C++ standard exception classes
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Implementation of C++ standard exception classes which must live in
*       the main CRT, not the C++ CRT, because they are referenced by RTTI
*       support in the main CRT.
*
*        exception
*          bad_cast
*          bad_typeid
*            __non_rtti_object
*
*******************************************************************************/

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifndef MRTDLL
#define MRTDLL
#endif

#ifndef _DLL
#define _DLL
#endif
#include "stdexcpt.cpp"
