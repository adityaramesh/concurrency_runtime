/***
*ehvecdtr.cxx - EH-aware version of destructor iterator helper function
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
#include <ehdata.h>
#include <eh.h>

#if defined (_M_CEE)
#define CALLTYPE __clrcall
#define CALEETYPE __clrcall
#define __RELIABILITY_CONTRACT \
    [System::Runtime::ConstrainedExecution::ReliabilityContract( \
        System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState, \
        System::Runtime::ConstrainedExecution::Cer::Success)]
#define ASSERT_UNMANAGED_CODE_ATTRIBUTE [System::Security::Permissions::SecurityPermissionAttribute(System::Security::Permissions::SecurityAction::Assert, UnmanagedCode = true)]
#define SECURITYCRITICAL_ATTRIBUTE [System::Security::SecurityCritical]
#define SECURITYSAFECRITICAL_ATTRIBUTE [System::Security::SecuritySafeCritical]
#else
#define CALEETYPE __stdcall
#define __RELIABILITY_CONTRACT
#define ASSERT_UNMANAGED_CODE_ATTRIBUTE
#define SECURITYCRITICAL_ATTRIBUTE
#define SECURITYSAFECRITICAL_ATTRIBUTE
#if defined (_M_IX86)
#define CALLTYPE __thiscall
#else
#define CALLTYPE __stdcall
#endif
#endif

#ifdef _WIN32

__RELIABILITY_CONTRACT
void CALEETYPE __ArrayUnwind(
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
);


__RELIABILITY_CONTRACT
SECURITYCRITICAL_ATTRIBUTE
void CALEETYPE __ehvec_dtor(
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
){
    _Analysis_assume_(count > 0);

    int success = 0;

    // Advance pointer past end of array
    ptr = (char*)ptr + size*count;

    __try
    {
        // Destruct elements
        while ( --count >= 0 )
        {
            ptr = (char*)ptr - size;
            (*pDtor)(ptr);
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            __ArrayUnwind(ptr, size, count, pDtor);
    }
}

__RELIABILITY_CONTRACT
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
static int ArrayUnwindFilter(EXCEPTION_POINTERS* pExPtrs)
{
    EHExceptionRecord *pExcept = (EHExceptionRecord*)pExPtrs->ExceptionRecord;

    switch(PER_CODE(pExcept))
    {
        case EH_EXCEPTION_NUMBER:
            terminate();
#ifdef ALLOW_UNWIND_ABORT
        case EH_ABORT_FRAME_UNWIND_PART:
            return EXCEPTION_EXECUTE_HANDLER;
#endif
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
}

__RELIABILITY_CONTRACT
SECURITYCRITICAL_ATTRIBUTE
void CALEETYPE __ArrayUnwind(
    void*       ptr,                // Pointer to array to destruct
    size_t      size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
){
    // 'unwind' rest of array

    __try
    {
        while ( --count >= 0 )
        {
            ptr = (char*) ptr - size;
            (*pDtor)(ptr);
        }
    }
    __except( ArrayUnwindFilter(exception_info()) )
    {
                ; // Deliberately do nothing
    }
}

#else

__RELIABILITY_CONTRACT
SECURITYCRITICAL_ATTRIBUTE
void CALEETYPE __ehvec_dtor(
    void*       ptr,                // Pointer to array to destruct
    unsigned    size,               // Size of each element (including padding)
    int         count,              // Number of elements in the array
    void(CALLTYPE *pDtor)(void*)    // The destructor to call
){
    // Advance pointer past end of array
    ptr = (char*)ptr + size*count;

    try
    {
        // Destruct elements
        while   ( --count >= 0 )
        {
            ptr = (char*)ptr - size;
            (*pDtor)(ptr);
        }
    }
    catch(...)
    {
        // If a destructor throws an exception, unwind the rest of this
        // array
        while ( --count >= 0 )
        {
            ptr = (char*) ptr - size;
            try {
                (*pDtor)(ptr);
            }
            catch(...)  {
                // If any destructor throws during unwind, terminate
                terminate();
            }
        }

        // After array is unwound, rethrow the exception so a user's handler
        // can handle it.
        throw;
    }
}

#endif
