/***
*tncleanup.c - Implementation of __clean_type_info_names for RTTI.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module provides an implementation of the __clean_type_info_names
*       which is responsible of cleaning up type names.
****/
#ifndef _TICORE
#define _TICORE
#endif  /* _TICORE */
#include <sect_attribs.h>
#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <typeinfo.h>
#include <mtdll.h>
#include <string.h>
#include <dbgint.h>
#include <undname.h>

__type_info_node __type_info_root_node;

extern "C" void __cdecl __clean_type_info_names_internal(__type_info_node * p_type_info_root_node);

/*
 * This routine is basically for cleanup of all the type_info::_M_data
 * lying around in the memory.
 */
/***
*void __cdecl __clean_type_info_names(void)
*
*Purpose:
*       This routine is used to clean up all the allocated typenames in the
*       module when linking to CRT dll. The reason behind adding this routine
*       is that the destructors of typeinfo classes are never called for
*       cleanup. And loading and unloading of this module may result in leaks.
*       This is not really needed in CRT dll of any other static library because
*       during unload, the crt heap will also be terminated and thus releasing
*       the memory.
*
*Entry:
*
*Exit:
*
*******************************************************************************/
extern "C" void __cdecl __clean_type_info_names(void)
{
     __clean_type_info_names_internal(&__type_info_root_node);
}
