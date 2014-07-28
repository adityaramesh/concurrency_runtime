/***
*mangdeh.cxx - The frame handler and everything associated with it.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       The frame handler and everything associated with it.
*
*       Entry points:
*       _CxxFrameHandler   - the frame handler.
*
*       Open issues:
*         Handling re-throw from dynamicly nested scope.
*         Fault-tolerance (checking for data structure validity).
****/

#ifdef CRTDLL
#undef CRTDLL
#endif

#ifdef MRTDLL
#undef MRTDLL
#endif

#ifndef _DLL
#define _DLL
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <windows.h>
#include <internal.h>
#include <mtdll.h>      // CRT internal header file
#include <ehassert.h>   // This project's versions of standard assert macros
#include <ehdata.h>     // Declarations of all types used for EH
#include <ehstate.h>    // Declarations of state management stuff
#include <eh.h>         // User-visible routines for eh
#include <ehhooks.h>    // Declarations of hook variables and callbacks
#include <trnsctrl.h>   // Routines to handle transfer of control (trnsctrl.asm)

#pragma hdrstop         // PCH is created from here

// Pre-V4 managed exception code
#define MANAGED_EXCEPTION_CODE  0XE0434F4D

// V4 and later managed exception code
#define MANAGED_EXCEPTION_CODE_V4  0XE0434352

////////////////////////////////////////////////////////////////////////////////
//
// Forward declaration of local functions:
//

// The local unwinder must be external (see ___CxxLongjmpUnwind in trnsctrl.cpp)

extern "C" _CRTIMP int __cdecl __FrameUnwindFilter(
    EXCEPTION_POINTERS *
);

extern "C" _CRTIMP void * __cdecl __AdjustPointer(
    void *,
    const PMD&
);

#if defined(_M_CEE_PURE)

//
// This describes the most recently handled exception, in case of a rethrow:
//
#define _pCurrentException      (*((EHExceptionRecord **)&(_getptd()->_curexception)))
#define _pCurrentExContext      (*((CONTEXT **)&(_getptd()->_curcontext)))
#define __ProcessingThrow       _getptd()->_ProcessingThrow

////////////////////////////////////////////////////////////////////////////////
//
// __TypeMatch - Check if the catch type matches the given throw conversion.
//
// Returns:
//     TRUE if the catch can catch using this throw conversion, FALSE otherwise.

template<typename HandlerType, typename CatchableType, typename ThrowInfo>
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYSAFECRITICAL_ATTRIBUTE
static int TypeMatch(
    HandlerType *pCatch,                // Type of the 'catch' clause
    CatchableType *pCatchable,          // Type conversion under consideration
    ThrowInfo *pThrow                   // General information about the thrown
                                        //   type.
) {
    // First, check for match with ellipsis:
    if (HT_IS_TYPE_ELLIPSIS(*pCatch)) {
        return TRUE;
    }

    // Not ellipsis; the basic types match if it's the same record *or* the
    // names are identical.
    if (HT_PTD(*pCatch) != CT_PTD(*pCatchable)
      && strcmp(HT_NAME(*pCatch), CT_NAME(*pCatchable)) != 0) {
        return FALSE;
    }

    // Basic types match.  The actual conversion is valid if:
    //   caught by ref if ref required *and*
    //   the qualifiers are compatible *and*
    //   the alignments match *and*
    //   the volatility matches

    return (!CT_BYREFONLY(*pCatchable) || HT_ISREFERENCE(*pCatch))
      && (!THROW_ISCONST(*pThrow) || HT_ISCONST(*pCatch))
#if defined(_M_X64) /*IFSTRIP=IGN*/
      && (!THROW_ISUNALIGNED(*pThrow) || HT_ISUNALIGNED(*pCatch))
#endif
      && (!THROW_ISVOLATILE(*pThrow) || HT_ISVOLATILE(*pCatch));
}

////////////////////////////////////////////////////////////////////////////////
//
// BuildCatchObjectHelper - Copy or construct the catch object from the object thrown.
//
// Returns:
//     0 if nothing to be done for constructing object from caller side
//     1 if single parameter constructor is to be called.
//     2 if two parameter constructor ist to be called.
//
// Side-effects:
//     A buffer in the subject function's frame is initialized.
//
// Open issues:
//     What happens if the constructor throws?  (or faults?)

template<typename HandlerType, typename CatchableType>
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
static int BuildCatchObjectHelper(
    EHExceptionRecord *pExcept,         // Original exception thrown
    void *pRN,                          // This is a pointer to the object
                                        // that we want to build while doing
                                        // COM+ eh. If we are in our own eh,
                                        // then this is a Registration node of
                                        // catching function
    HandlerType *pCatch,                // The catch clause that got it
    CatchableType *pConv                // The conversion to use
) {
    int retval = 0;
    EHTRACE_ENTER;

    // If the catch is by ellipsis, then there is no object to construct.
    // If the catch is by type(No Catch Object), then leave too!
    if (HT_IS_TYPE_ELLIPSIS(*pCatch) ||
        (!HT_DISPCATCH(*pCatch) && !HT_ISCOMPLUSEH(*pCatch))) {
        EHTRACE_EXIT;
        return 0;
    }

    DASSERT(HT_ISCOMPLUSEH(*pCatch));
    void **pCatchBuffer = (void **)pRN;
    __try {
        if (HT_ISREFERENCE(*pCatch)) {

            // The catch is of form 'reference to T'.  At the throw point we
            // treat both 'T' and 'reference to T' the same, i.e.
            // pExceptionObject is a (machine) pointer to T.  Adjust as
            // required.
            if (_ValidateRead(PER_PEXCEPTOBJ(pExcept))
              && _ValidateWrite(pCatchBuffer)) {
                *pCatchBuffer = PER_PEXCEPTOBJ(pExcept);
                *pCatchBuffer = __AdjustPointer(*pCatchBuffer,
                  CT_THISDISP(*pConv));
            } else {
                _inconsistency(); // Does not return; TKB
            }
        } else if (CT_ISSIMPLETYPE(*pConv)) {

            // Object thrown is of simple type (this including pointers) copy
            // specified number of bytes.  Adjust the pointer as required.  If
            // the thing is not a pointer, then this should be safe since all
            // the entries in the THISDISP are 0.
            if (_ValidateRead(PER_PEXCEPTOBJ(pExcept))
              && _ValidateWrite(pCatchBuffer)) {
                memmove(pCatchBuffer, PER_PEXCEPTOBJ(pExcept), CT_SIZE(*pConv));

                if (CT_SIZE(*pConv) == sizeof(void*) && *pCatchBuffer != NULL) {
                    *pCatchBuffer = __AdjustPointer(*pCatchBuffer,
                      CT_THISDISP(*pConv));
                }
            } else {
                _inconsistency(); // Does not return; TKB
            }
        } else {

            // Object thrown is UDT.
            if (CT_COPYFUNC(*pConv) == NULL) {

                // The UDT had a simple ctor.  Adjust in the thrown object,
                // then copy n bytes.
                if (_ValidateRead(PER_PEXCEPTOBJ(pExcept))
                  && _ValidateWrite(pCatchBuffer)) {
                    memmove(pCatchBuffer, __AdjustPointer(PER_PEXCEPTOBJ(pExcept),
                      CT_THISDISP(*pConv)), CT_SIZE(*pConv));
                } else {
                    _inconsistency(); // Does not return; TKB
                }
            } else {

                // It's a UDT: make a copy using copy ctor

#pragma warning(disable:4191)

                if (_ValidateRead(PER_PEXCEPTOBJ(pExcept))
                  && _ValidateWrite(pCatchBuffer)
                  && _ValidateExecute((FARPROC)CT_COPYFUNC(*pConv))) {

#pragma warning(default:4191)

                    if (CT_HASVB(*pConv)) {
                        retval = 2;
                    } else {
                        retval = 1;
                    }
                } else {
                    _inconsistency(); // Does not return; TKB
                }
            }
        }
    } __except(EHTRACE_EXCEPT(EXCEPTION_EXECUTE_HANDLER))
        {
        // Something went wrong when building the catch object.
        terminate();
    }

    EHTRACE_EXIT;
    return retval;
}
////////////////////////////////////////////////////////////////////////////////
//
// BuildCatchObject - Copy or construct the catch object from the object thrown.
//
// Returns:
//     nothing.
//
// Side-effects:
//     A buffer in the subject function's frame is initialized.
//
// Open issues:
//     What happens if the constructor throws?  (or faults?)

template<typename HandlerType, typename CatchableType>
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
static void BuildCatchObject(
    EHExceptionRecord *pExcept,         // Original exception thrown
    void *pRN,                          // This is a pointer to the object
                                        // that we want to build while doing
                                        // COM+ eh. If we are in our own eh,
                                        // then this is a Registration node of
                                        // catching function
    HandlerType *pCatch,                // The catch clause that got it
    CatchableType *pConv                // The conversion to use
) {
    EHTRACE_ENTER;

    void **pCatchBuffer = (void **)pRN;

    DASSERT(HT_ISCOMPLUSEH(*pCatch));

    __try {
        switch(BuildCatchObjectHelper(pExcept, pRN, pCatch, pConv))
        {
            case 1:
                {
                    void (__clrcall *pFunc)(void *, void *) = (void (__clrcall *)(void *, void *))(void *)CT_COPYFUNC(*pConv);
                    pFunc((void *)pCatchBuffer, __AdjustPointer(PER_PEXCEPTOBJ(pExcept), CT_THISDISP(*pConv)));
                }
                break;
            case 2:
                {
                    void (__clrcall *pFunc)(void *, void *, int) = (void (__clrcall *)(void *, void *, int))(void *)CT_COPYFUNC(*pConv);
                    pFunc((void *)pCatchBuffer, __AdjustPointer(PER_PEXCEPTOBJ(pExcept), CT_THISDISP(*pConv)), 1);
                }
                break;
            case 0:
                break;
            default:
                break;
        }
    } __except(EHTRACE_EXCEPT(EXCEPTION_EXECUTE_HANDLER))
        {
        // Something went wrong when building the catch object.
        terminate();
    }

    EHTRACE_EXIT;
}



////////////////////////////////////////////////////////////////////////////////
//
// __DestructExceptionObject_m - Call the destructor (if any) of the original
//   exception object.
//
// Returns: None.
//
// Side-effects:
//     Original exception object is destructed.
//
// Notes:
//     If destruction throws any exception, and we are destructing the exception
//       object as a result of a new exception, we give up.  If the destruction
//       throws otherwise, we let it be.

[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
static void __DestructExceptionObject_m(
    EHExceptionRecord *pExcept,         // The original exception record
    BOOLEAN fThrowNotAllowed            // TRUE if destructor not allowed to
                                        //   throw
) {
    EHTRACE_ENTER_FMT1("Destroying object @ 0x%p", PER_PEXCEPTOBJ(pExcept));

    if (pExcept != NULL && THROW_UNWINDFUNC(*PER_PTHROW(pExcept)) != NULL) {

        __try {
            void (__clrcall * pDtor)(void*) = NULL;

#pragma warning(disable:4191)
            pDtor = (void (__clrcall *)(void *))(THROW_UNWINDFUNC(*PER_PTHROW(pExcept)));
            (*pDtor)(PER_PEXCEPTOBJ(pExcept));
#pragma warning(default:4191)

        } __except(EHTRACE_EXCEPT(fThrowNotAllowed
          ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)) {

            // Can't have new exceptions when we're unwinding due to another
            // exception.
            terminate();
        }
    }

    EHTRACE_EXIT;
}
////////////////////////////////////////////////////////////////////////////////
// Model of C++ eh in COM+
//
// void func()
// {
//     try {
//         TryBody();
//     } catch (cpp_object o)
//     {
//         CatchOBody();
//     } catch (...)
//     {
//         CatchAllBody();
//     }
// }
//
// Turns into this:
//
//
// void func()
// {
//     int rethrow;
//     // One per try block
//     int isCxxException;
//     // One per catch(...)
//     __try {
//         TryBody();
//     }
//     __except(___CxxExceptionFilter(exception,
//                                   typeinfo(cpp_object),
//                                   flags,
//                                   &o))
//     // This is how it's done already
//     {
//     // Begin catch(object) prefix
//     char *storage = _alloca(___CxxQueryExceptionSize());
//     rethrow = false;
//     ___CxxRegisterExceptionObject(exception,
//                                  storage);
//     __try {
//         __try {
//             // End catch(object) prefix
//             CatchOBody();
//             // Begin catch(object) suffix
//         } __except(rethrow = ___CxxDetectRethrow(exception),
//                    EXCEPTION_CONTINUE_SEARCH)
//         {}
//     }
//     __finally
//     {
//         ___CxxUnregisterExceptionObject(storage,
//                                        rethrow);
//     }
//     // End catch(object) suffix
//     }
//     __except(1)
//     {
//         // Begin catch(...) prefix
//         char *storage = _alloca(___CxxQueryExceptionSize());
//         rethrow = false;
//         isCxxException = ___CxxRegisterExceptionObject(exception,
//                                                       storage);
//         __try
//         {
//             __try
//             {
//             // End catch(...) prefix
//             CatchAllBody();
//             // Begin catch(...) suffix
//         } __except(rethrow = ___CxxDetectRethrow(exception),
//                    EXCEPTION_CONTINUE_SEARCH)
//         {}
//     } __finally
//     {
//         if (isCxxException)
//         ___CxxUnregisterExceptionObject(storage, rethrow);
//     }
//     // End catch(...) suffix
//     }
// }
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxExceptionFilter() - Returns EXCEPTION_EXECUTE_HANDLER when the pType
//                          matches with the objects we can catch. Returns
//                          EXCEPTION_CONTINUE_SEARCH when pType is not one of
//                          the catchable type for the thrown object. This
//                          function is made for use with COM+ EH, where they
//                          attempt to do C++ EH as well.
//

[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
SECURITYCRITICAL_ATTRIBUTE
int __clrcall ___CxxExceptionFilter(
    void *ppExcept,                     // Information for this (logical)
                                        // exception
    void *pType,                        // Info about the datatype.
    int adjectives,                     // Extra Info about the datatype.
    void *pBuildObj                     // Pointer to datatype.
    )
{
#if defined(_M_X64)
    CatchableType * const __unaligned *ppCatchable;
#else
    CatchableType * const *ppCatchable;
#endif
    CatchableType *pCatchable;
    int catchables;
    EHExceptionRecord *pExcept;

    if (!ppExcept) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    pExcept = *(EHExceptionRecord **)ppExcept;
    // If catch all, always return EXCEPTION_EXECUTE_HANDLER
    // Note that if the adjective has HT_IsStdDotDot flag on, we comply Std
    // C++ eh behaviour, i.e. only catching C++ objects.
    if ( TD_IS_TYPE_ELLIPSIS((TypeDescriptor *)pType) &&
         (pExcept->ExceptionCode == MANAGED_EXCEPTION_CODE ||
          pExcept->ExceptionCode == MANAGED_EXCEPTION_CODE_V4 ||
          !(adjectives & HT_IsStdDotDot)))
    {
        if (PER_IS_MSVC_EH(pExcept))
        {
            if ( PER_PTHROW(pExcept) == NULL)
            {
                if ( _pCurrentException == NULL) {
                    return EXCEPTION_CONTINUE_SEARCH;
                }
            }
        }
        __ProcessingThrow++;
        return EXCEPTION_EXECUTE_HANDLER;
    }

    if (PER_IS_MSVC_EH(pExcept))
    {
        if ( PER_PTHROW(pExcept) == NULL) {
            if (_pCurrentException == NULL)
                return EXCEPTION_CONTINUE_SEARCH;
            pExcept =  _pCurrentException;
        }

        /*
         * Note that we will only be using pType or dispType
         * and adjective field of pCatch.
         */
        struct _s_HandlerType HType;
        HType.pType = (TypeDescriptor *)pType;
        HType.adjectives = adjectives;
        SET_HT_ISCOMPLUSEH(HType);

        // Scan all types that thrown object can be converted to:
        ppCatchable = THROW_CTLIST(*PER_PTHROW(pExcept));
        for (catchables = THROW_COUNT(*PER_PTHROW(pExcept));
          catchables > 0; catchables--, ppCatchable++) {

            pCatchable = *ppCatchable;
            if (TypeMatch(&HType, pCatchable, PER_PTHROW(pExcept))) {
                // SucessFull. Now build the object.
                __ProcessingThrow++;
                if (pBuildObj != NULL)
                    BuildCatchObject(pExcept, pBuildObj, &HType, pCatchable);
                return EXCEPTION_EXECUTE_HANDLER;
            }
        } // Scan posible conversions
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxRgisterExceptionObject() - Registers Exception Object and saves it to
//                                 This is same as first part of
//                                 CallCatchBlock.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
int __clrcall ___CxxRegisterExceptionObject(
    void *ppExcept,
    void *pStorage
)
{
    // This function is only called for C++ EH.
    EHExceptionRecord *pExcept = NULL;
    FRAMEINFO *pFrameInfo = (FRAMEINFO *)pStorage;
    EHExceptionRecord **ppSaveException;
    CONTEXT **ppSaveExContext;
    ppSaveException = (EHExceptionRecord **)(&pFrameInfo[1]);
    ppSaveExContext = (CONTEXT **)(&ppSaveException[1]);
    if (ppExcept  != NULL && (*(void **)ppExcept) != NULL) {
        pExcept = *(EHExceptionRecord **)ppExcept;
        if (PER_IS_MSVC_EH(pExcept)) {
            if ( PER_PTHROW(pExcept) == NULL) {
                // was a rethrow
                pExcept = _pCurrentException;
            }
        }
        pFrameInfo = _CreateFrameInfo(pFrameInfo, PER_PEXCEPTOBJ(pExcept));
        *ppSaveException = _pCurrentException;
        *ppSaveExContext = _pCurrentExContext;
        _pCurrentException = pExcept;
    } else {
        *ppSaveException = (EHExceptionRecord *)-1;
        *ppSaveExContext = (CONTEXT *)-1;
    }
    __ProcessingThrow--;
    if ( __ProcessingThrow < 0)
        __ProcessingThrow = 0;
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxDetectRethrow() - Looks at the Exception and returns true if rethrow,
//                        false if not a rethrow. This is then used for
//                        destructing the exception object in
//                        ___CxxUnregisterExceptionObject().
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
SECURITYCRITICAL_ATTRIBUTE
int __clrcall ___CxxDetectRethrow(
    void *ppExcept
)
{
    EHExceptionRecord *pExcept;
    if (!ppExcept)
        return EXCEPTION_CONTINUE_SEARCH;
    pExcept = *(EHExceptionRecord **)ppExcept;
    if (PER_IS_MSVC_EH(pExcept) && PER_PTHROW(pExcept) == NULL) {
        __ProcessingThrow++;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxUnregisterExceptionObject - Destructs Exception Objects if rethrow ==
//                          true. Also set __pCurrentException and
//                          __pCurrentExContext() to current value.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
void __clrcall ___CxxUnregisterExceptionObject(
    void *pStorage,
    int rethrow
)
{
    FRAMEINFO *pFrameInfo = (FRAMEINFO *)pStorage;
    EHExceptionRecord **ppSaveException;
    CONTEXT **ppSaveExContext;
    ppSaveException = (EHExceptionRecord **)(&pFrameInfo[1]);
    ppSaveExContext = (CONTEXT **)(&ppSaveException[1]);
    if (*ppSaveException != (void *)-1) {
        _FindAndUnlinkFrame(pFrameInfo);
        if ( !rethrow && PER_IS_MSVC_EH(_pCurrentException) &&
                _IsExceptionObjectToBeDestroyed(PER_PEXCEPTOBJ(_pCurrentException))
                ) {
            __DestructExceptionObject_m(_pCurrentException, TRUE);
        }
        if (PER_IS_MSVC_EH(_pCurrentException) && rethrow)
            __ProcessingThrow--;
        _pCurrentException = *ppSaveException;
        _pCurrentExContext = *ppSaveExContext;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxQueryExceptionSize - returns the value of Storage needed to save
//                          FrameInfo + two pointers.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
int __clrcall ___CxxQueryExceptionSize(
    void
)
{
    return sizeof(FRAMEINFO) + sizeof(void *) + sizeof(void *);
}

#endif // defined(_M_CEE_PURE)

////////////////////////////////////////////////////////////////////////////////
//
// The following routines are available in both the pure and mixed CRTs
// (msvcurt and msvcmrt).  They are wrappers around calls to dtors which
// call terminate() when those dtors throw an exception.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxCallUnwindDtor - Calls a destructor during unwind. For COM+, the dtor
//                       call needs to be wrapped inside a __try/__except to
//                       get correct terminate() behavior when an exception
//                       occurs during the dtor call.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
extern "C" void __clrcall ___CxxCallUnwindDtor(
    void (__clrcall * pDtor)(void *),
    void *pThis
)
{
    __try
    {
        (*pDtor)(pThis);
    }
    __except(__FrameUnwindFilter(exception_info()))
    {
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// ___CxxCallUnwindDelDtor - Calls a delete during unwind. For COM+, the dtor
//                       call needs to be wrapped inside a __try/__except to
//                       get correct terminate() behavior when an exception
//                       occurs during the dtor call.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
extern "C" void __clrcall ___CxxCallUnwindDelDtor(
    void (__clrcall * pDtor)(void*),
    void *pThis
)
{
    __try
    {
        (*pDtor)(pThis);
    }
    __except(__FrameUnwindFilter(exception_info()))
    {
        ; // Deliberately do nothing
    }
}

//////////////////////////////////////////////////////////////////////////////////
// ___CxxCallUnwindVecDtor - Calls a vector destructor during vector unwind.
//                       For COM+, the dtor call needs to be wrapped inside
//                       a __try/__except to get correct terminate() behavior
//                       when an exception occurs during the dtor call.
//
[System::Runtime::ConstrainedExecution::ReliabilityContract(
    System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState,
    System::Runtime::ConstrainedExecution::Cer::Success)]
ASSERT_UNMANAGED_CODE_ATTRIBUTE
SECURITYCRITICAL_ATTRIBUTE
extern "C" void __clrcall ___CxxCallUnwindVecDtor(
    void (__clrcall * pVecDtor)(void*, size_t, int, void (__clrcall *)(void*)),
    void* ptr,
    size_t size,
    int count,
    void (__clrcall * pDtor)(void*)
)
{
    __try
    {
        (*pVecDtor)(ptr, size, count, pDtor);
    }
    __except(__FrameUnwindFilter(exception_info()))
    {
    }
}
