/***
*heapinit.c -  Initialize the heap
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <stdlib.h>
#include <winheap.h>
#include <internal.h>

HANDLE _crtheap=NULL;

/***
*_heap_init() - Initialize the heap
*
*Purpose:
*       Setup the initial C library heap.
*
*       NOTES:
*       (1) This routine should only be called once!
*       (2) This routine must be called before any other heap requests.
*
*Entry:
*       <void>
*Exit:
*       Returns 1 if successful, 0 otherwise.
*
*Exceptions:
*       If heap cannot be initialized, the program will be terminated
*       with a fatal runtime error.
*
*******************************************************************************/

int __cdecl _heap_init (void)
{
        //  Initialize the "big-block" heap first.
        if ( (_crtheap = GetProcessHeap()) == NULL )
            return 0;

        return 1;
}

/***
*_heap_term() - return the heap to the OS
*
*Purpose:
*
*       NOTES:
*       (1) This routine should only be called once!
*       (2) This routine must be called AFTER any other heap requests.
*
*Entry:
*       <void>
*Exit:
*       <void>
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _heap_term (void)
{
        _crtheap=NULL;
}

/***
*_get_heap_handle() - Get the HANDLE to the Win32 system heap used by the CRT
*
*Purpose:
*       Retrieve the HANDLE to the Win32 system heap used by the CRT.
*
*Entry:
*       <void>
*Exit:
*       Returns the CRT heap HANDLE as an intptr_t
*
*Exceptions:
*
*******************************************************************************/

intptr_t __cdecl _get_heap_handle(void)
{
    _ASSERTE(_crtheap);
    return (intptr_t)_crtheap;
}
