/***
*mtypinfo.cpp - Implementation of type_info for RTTI.
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   This module provides an implementation of the class type_info
*   for Run-Time Type Information (RTTI) in the managed runtime
*   DLL.
****/

#ifdef CRTDLL
#undef CRTDLL
#endif  /* CRTDLL */

#ifndef MRTDLL
#define MRTDLL
#endif  /* MRTDLL */

#ifndef _DLL
#define _DLL
#endif  /* _DLL */
#include "typinfo.cpp"

