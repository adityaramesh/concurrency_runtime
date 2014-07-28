/***
*ehvcccvb.cpp - EH copy-ctor iterator helper function for class w/ virtual bases
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       These functions are called when constructing and destructing arrays of
*       objects.  Their purpose is to assure that constructed elements get
*       destructed if the constructor for one of the elements throws.
*
*       Must be compiled using "-d1Binl" to be able to specify __thiscall
*       at the user level
****/

#include <cruntime.h>
#include <eh.h>

#if defined (_M_CEE)
#define CALLTYPE __clrcall
#define CALEETYPE __clrcall
#define __RELIABILITY_CONTRACT \
    [System::Runtime::ConstrainedExecution::ReliabilityContract( \
        System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState, \
        System::Runtime::ConstrainedExecution::Cer::Success)]
#else  /* defined (_M_CEE) */
#define CALEETYPE __stdcall
#define __RELIABILITY_CONTRACT
#if defined (_M_IX86)
#define CALLTYPE __thiscall
#else  /* defined (_M_IX86) */
#define CALLTYPE __stdcall
#endif  /* defined (_M_IX86) */
#endif  /* defined (_M_CEE) */

#ifdef _WIN32

__RELIABILITY_CONTRACT
void CALEETYPE __ArrayUnwind(
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
);


__RELIABILITY_CONTRACT
void CALEETYPE __ehvec_copy_ctor_vb(
    void*       dst,                // Pointer to destination array
    void*       src,                // Pointer to source array
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pCopyCtor)(void*,void*),   // Constructor to call
    void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i = 0;      // Count of elements constructed
    int success = 0;

    __try
    {
        // Construct the elements of the array
        for( ;  i < count;  i++ )
        {
#pragma warning(disable:4191)

            (*(void(CALLTYPE*)(void*,void*,int))pCopyCtor)( dst, src, 1 );

#pragma warning(default:4191)

            dst = (char*)dst + size;
            src = (char*)src + size;
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            __ArrayUnwind(dst, size, i, pDtor);
    }
}

#else  /* _WIN32 */

__RELIABILITY_CONTRACT
void CALEETYPE __ehvec_copy_ctor_vb(
    void*       dst,                // Pointer to destination array
    void*       src,                // Pointer to source array
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pCopyCtor)(void*, void*),   // Constructor to call
    void(CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
){
    int i;  // Count of elements constructed

    try
    {
        // Construct the elements of the array
        for( i = 0;  i < count;  i++ )
        {
            (*pCopyCtor)( dst, src );
            dst = (char*)dst + size;
            src = (char*)src + size;
        }
    }
    catch(...)
    {
        // If a constructor throws, unwind the portion of the array thus
        // far constructed.
        for( i--;  i >= 0;  i-- )
        {
            dst = (char*)dst - size;
            try {
#pragma warning(disable:4191)

            (*(void(CALLTYPE*)(void*,void*,int))pCopyCtor)( dst, src, 1 );

#pragma warning(default:4191)
            }
            catch(...) {
                // If the destructor threw during the unwind, quit
                terminate();
            }
        }

        // Propagate the exception to surrounding frames
        throw;
    }
}

#endif  /* _WIN32 */
