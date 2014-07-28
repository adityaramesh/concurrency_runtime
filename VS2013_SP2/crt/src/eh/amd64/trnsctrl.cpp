/***
*trnsctrl.cpp -
*
*       Copyright (c) 1990-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
****/

extern "C" {
#include <windows.h>
};
#include <winnt.h>
#include <mtdll.h>

#include <ehassert.h>
#include <ehdata.h>
#include <trnsctrl.h>
#include <ehstate.h>
#include <eh.h>
#include <ehhooks.h>
#include <stdlib.h>
#include <internal.h>
#pragma hdrstop

#define pFrameInfoChain   (*((FRAMEINFO **)    &(_getptd()->_pFrameInfoChain)))
#define _ImageBase        (_getptd()->_ImageBase)
#define _ThrowImageBase   (_getptd()->_ThrowImageBase)
#define _pCurrentException (*((EHExceptionRecord **)&(_getptd()->_curexception)))
#define _pForeignExcept     (*((EHExceptionRecord **)&(_getptd()->_pForeignException)))

extern "C" void __FrameUnwindToState(EHRegistrationNode *, DispatcherContext *, FuncInfo *, __ehstate_t);

//
// Returns the establisher frame pointers. For catch handlers it is the parent's frame pointer.
//
EHRegistrationNode *_GetEstablisherFrame(
    EHRegistrationNode  *pRN,
    DispatcherContext   *pDC,
    FuncInfo            *pFuncInfo,
    EHRegistrationNode  *pEstablisher
) {
    TryBlockMapEntry *pEntry;
    HandlerType *pHandler;
    unsigned __int64 HandlerAdd, ImageBase;
    unsigned num_of_try_blocks = FUNC_NTRYBLOCKS(*pFuncInfo);
    unsigned index, i;
    __ehstate_t curState;

    curState = __StateFromControlPc(pFuncInfo, pDC);
    *pEstablisher = *pRN;
    for (index = num_of_try_blocks; index > 0; index--) {
        pEntry = FUNC_PTRYBLOCK(*pFuncInfo, index -1, pDC->ImageBase);
        if (curState > TBME_HIGH(*pEntry) && curState <= TBME_CATCHHIGH(*pEntry)) {
            // Get catch handler address.
            HandlerAdd = (*RtlLookupFunctionEntry(pDC->ControlPc, &ImageBase, NULL)).BeginAddress;
            pHandler = TBME_PLIST(*pEntry, ImageBase);
            for ( i = 0;
                  i < (unsigned)TBME_NCATCHES(*pEntry) &&
                  pHandler[i].dispOfHandler != HandlerAdd
                  ; i++);
            if ( i < (unsigned)TBME_NCATCHES(*pEntry)) {
                *pEstablisher = *(EHRegistrationNode *)OffsetToAddress(pHandler[i].dispFrame, *pRN);
                break;
            }
        }
    }
    return pEstablisher;
}

extern "C" _CRTIMP unsigned __int64 __cdecl _GetImageBase()
{
    return _ImageBase;
}

extern "C" _CRTIMP unsigned __int64 __cdecl _GetThrowImageBase()
{
    return _ThrowImageBase;
}

extern "C" VOID __cdecl _SetImageBase(unsigned __int64 ImageBaseToRestore)
{
    _ImageBase = ImageBaseToRestore;
}

extern "C" _CRTIMP VOID __cdecl _SetThrowImageBase(unsigned __int64 NewThrowImageBase)
{
    _ThrowImageBase = NewThrowImageBase;
}

extern "C" VOID _MoveContext(CONTEXT* pTarget, CONTEXT* pSource)
{
    RtlMoveMemory(pTarget, pSource, sizeof(CONTEXT));
}

// This function returns the try block for the given state if the state is in a
// catch; otherwise, NULL is returned.

static __inline TryBlockMapEntry *_CatchTryBlock(
    FuncInfo            *pFuncInfo,
    __ehstate_t         curState
) {
    TryBlockMapEntry *pEntry;
    unsigned num_of_try_blocks = FUNC_NTRYBLOCKS(*pFuncInfo);
    unsigned index;

    for (index = num_of_try_blocks; index > 0; index--) {
        pEntry = FUNC_PTRYBLOCK(*pFuncInfo, index -1, _ImageBase);
        if (curState > TBME_HIGH(*pEntry) && curState <= TBME_CATCHHIGH(*pEntry)) {
            return pEntry;
        }
    }

    return NULL;
}

//
// This routine returns TRUE if we are executing from within a catch.  Otherwise, FALSE is returned.
//

BOOL _ExecutionInCatch(
    DispatcherContext   *pDC,
    FuncInfo            *pFuncInfo
) {
    __ehstate_t curState =  __StateFromControlPc(pFuncInfo, pDC);
    return _CatchTryBlock(pFuncInfo, curState)? TRUE : FALSE;
}

// The name of this function is rather misleading. This function won't really unwind
// to empty state. This function will unwind to lowest possible state for current block.
// Here is an example
//
// try {
//   // State = 1;
// } catch (...) {
//   // State when we enter catch is 2
//   // State inside catch is 3-5;
// }
// if __FrameUnwindToEmptyState is called for the main function, the target state will
// be -1 but if __FrameUnwindToEmptyState is called for catch block, the target state for
// __FrameUnwindToState will be 2 not -1. This way we are able to unwind the stack block
// by block not the whole function in single call.

VOID __FrameUnwindToEmptyState(
    EHRegistrationNode *pRN,
    DispatcherContext  *pDC,
    FuncInfo           *pFuncInfo
) {
    __ehstate_t         stateFromControlPC;
    TryBlockMapEntry    *pEntry;
    EHRegistrationNode  EstablisherFramePointers, *pEstablisher;

    pEstablisher = _GetEstablisherFrame(pRN, pDC, pFuncInfo, &EstablisherFramePointers);
    stateFromControlPC = __StateFromControlPc(pFuncInfo, pDC);
    pEntry = _CatchTryBlock(pFuncInfo, stateFromControlPC);

    __FrameUnwindToState(pEstablisher, pDC, pFuncInfo,
                         pEntry == NULL ? EH_EMPTY_STATE : TBME_HIGH(*pEntry));
}

//
// Prototype for the internal handler
//
extern "C" EXCEPTION_DISPOSITION __InternalCxxFrameHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    CONTEXT            *pContext,       // Context info
    DispatcherContext  *pDC,            // More dynamic info for this frame
    FuncInfo           *pFuncInfo,      // Static information for this frame
    int                CatchDepth,      // How deeply nested are we?
    EHRegistrationNode *pMarkerRN,      // Marker node for when checking inside catch block
    BOOL                recursive);     // True if this is a translation exception

//
// __CxxFrameHandler3 - Real entry point to the runtime
//                                              __CxxFrameHandler2 is an alias for __CxxFrameHandler3
//                                              since they are compatible in VC version of CRT
//                      These function should be separated out if a change makes
//                                              __CxxFrameHandler3 incompatible with __CxxFrameHandler2
//
extern "C" _CRTIMP EXCEPTION_DISPOSITION __CxxFrameHandler3(
    EHExceptionRecord  *pExcept,         // Information for this exception
    EHRegistrationNode RN,               // Dynamic information for this frame
    CONTEXT            *pContext,        // Context info
    DispatcherContext  *pDC              // More dynamic info for this frame
) {
    FuncInfo                *pFuncInfo;
    EXCEPTION_DISPOSITION   result;
    EHRegistrationNode      EstablisherFrame = RN;

    _ImageBase = pDC->ImageBase;
    _ThrowImageBase = (unsigned __int64)pExcept->params.pThrowImageBase;
    pFuncInfo = (FuncInfo*)(_ImageBase +*(PULONG)pDC->HandlerData);
    result = __InternalCxxFrameHandler( pExcept, &EstablisherFrame, pContext, pDC, pFuncInfo, 0, NULL, FALSE );
    return result;
}

//
// __CxxFrameHandler2 - Remove after compiler is updated
//
extern "C" _CRTIMP EXCEPTION_DISPOSITION __CxxFrameHandler2(
    EHExceptionRecord  *pExcept,         // Information for this exception
    EHRegistrationNode RN,               // Dynamic information for this frame
    CONTEXT            *pContext,        // Context info
    DispatcherContext  *pDC              // More dynamic info for this frame
) {
    return __CxxFrameHandler3(pExcept, RN, pContext, pDC);
}

extern "C" _CRTIMP EXCEPTION_DISPOSITION __CxxFrameHandler(
    EHExceptionRecord  *pExcept,         // Information for this exception
    EHRegistrationNode RN,               // Dynamic information for this frame
    CONTEXT            *pContext,        // Context info
    DispatcherContext  *pDC              // More dynamic info for this frame
) {
    FuncInfo                *pFuncInfo;
    EXCEPTION_DISPOSITION   result;
    EHRegistrationNode      EstablisherFrame = RN;

    _ImageBase = pDC->ImageBase;
    _ThrowImageBase = (unsigned __int64)pExcept->params.pThrowImageBase;
    pFuncInfo = (FuncInfo*)(_ImageBase +*(PULONG)pDC->HandlerData);
    result = __InternalCxxFrameHandler( pExcept, &EstablisherFrame, pContext, pDC, pFuncInfo, 0, NULL, FALSE );
    return result;
}

// Call the SEH to EH translator.
int __SehTransFilter( EXCEPTION_POINTERS    *ExPtrs,
                      EHExceptionRecord     *pExcept,
                      EHRegistrationNode    *pRN,
                      CONTEXT               *pContext,
                      DispatcherContext     *pDC,
                      FuncInfo              *pFuncInfo,
                      BOOL                  *pResult)
{
        _pForeignExcept = pExcept;
        _ThrowImageBase = (unsigned __int64)((EHExceptionRecord *)ExPtrs->ExceptionRecord)->params.pThrowImageBase;
        __InternalCxxFrameHandler( (EHExceptionRecord *)ExPtrs->ExceptionRecord,
                                   pRN,
                                   pContext,
                                   pDC,
                                   pFuncInfo,
                                   0,
                                   NULL,
                                   TRUE );
        _pForeignExcept = NULL;
        *pResult = TRUE;
        return EXCEPTION_EXECUTE_HANDLER;
}

BOOL _CallSETranslator(
    EHExceptionRecord   *pExcept,    // The exception to be translated
    EHRegistrationNode  *pRN,        // Dynamic info of function with catch
    CONTEXT             *pContext,   // Context info
    DispatcherContext   *pDC,        // More dynamic info of function with catch (ignored)
    FuncInfo            *pFuncInfo,  // Static info of function with catch
    ULONG               CatchDepth,  // How deeply nested in catch blocks are we?
    EHRegistrationNode  *pMarkerRN   // Marker for parent context
) {
    BOOL result = FALSE;
    pRN;
    pDC;
    pFuncInfo;
    CatchDepth;

    // Call the translator.

    _EXCEPTION_POINTERS excptr = { (PEXCEPTION_RECORD)pExcept, pContext };

    __try {
        _se_translator_function pSETranslator;
        pSETranslator = __pSETranslator;
        pSETranslator(PER_CODE(pExcept), &excptr);
        result = FALSE;
    } __except(__SehTransFilter( exception_info(),
                                 pExcept,
                                 pRN,
                                 pContext,
                                 pDC,
                                 pFuncInfo,
                                 &result
                                )) {}

    // If we got back, then we were unable to translate it.

    return result;
}

/////////////////////////////////////////////////////////////////////////////
//
// _GetRangeOfTrysToCheck - determine which try blocks are of interest.
//
// The try blocks of intrests are the ones in current catch block or function block.
// We will not try to call the catch that are outside the scope of this block. Consider
// the example.
//
// foo() {
//   try {  // Try block 1
//     try { // Try block 2
//       throw 1;  // Throw1
//     } catch (int) { // Catch block 1
//       try { // Try block 3
//         throw 1; // Throw2
//       } catch (int) { // Catch block 2
//         throw;   // Throw3
//       }
//     }
//   } catch (int) { // Catch block 3
//   }
// }
//
// When we have exception from Throw1, our block of concern is function foo. Here the
// try blocks of intrest are Try block 2 and Try block 1.
//
// When we have exception for Throw2, we are in Catch block 1 and thus try block of
// concern is Try block 3. We don't really need to care about Try block 1 here as
// we have the main function block still on the stack and exception handler will also
// be called for that block also.
//
// Returns:
//      Address of first try block of interest is returned
//      pStart and pEnd get the indices of the range in question
//
TryBlockMapEntry* _GetRangeOfTrysToCheck(
        EHRegistrationNode  *pRN,
        FuncInfo            *pFuncInfo,
        int                 CatchDepth,
        __ehstate_t         curState,
        unsigned            *pStart,
        unsigned            *pEnd,
        DispatcherContext   *pDC
) {
    TryBlockMapEntry *pEntry, *pCurCatchEntry = NULL;
    unsigned num_of_try_blocks = FUNC_NTRYBLOCKS(*pFuncInfo);
    unsigned int index;
    __ehstate_t ipState = __StateFromControlPc(pFuncInfo, pDC);

    DASSERT( num_of_try_blocks > 0 );

    *pStart = *pEnd = -1;
    for (index = num_of_try_blocks; index > 0; index--) {
        pEntry = FUNC_PTRYBLOCK(*pFuncInfo, index -1, pDC->ImageBase);
        if (ipState > TBME_HIGH(*pEntry) && ipState <= TBME_CATCHHIGH(*pEntry)) {
            break;
        }
    }
    if (index) {
        pCurCatchEntry = FUNC_PTRYBLOCK(*pFuncInfo, index -1, pDC->ImageBase);
    }
    for(index = 0; index < num_of_try_blocks; index++ ) {
        pEntry = FUNC_PTRYBLOCK(*pFuncInfo, index, pDC->ImageBase);
        // if in catch block, check for try-catch only in current block
        if (pCurCatchEntry) {
            if (TBME_LOW(*pEntry) <= TBME_HIGH(*pCurCatchEntry) ||
                TBME_HIGH(*pEntry) > TBME_CATCHHIGH(*pCurCatchEntry))
                continue;
        }
        if( curState >= TBME_LOW(*pEntry) && curState <= TBME_HIGH(*pEntry) ) {
           if (*pStart == -1) {
               *pStart = index;
           }
           *pEnd = index+1;
        }
    }

    if ( *pStart == -1){
        *pStart = 0;
        *pEnd = 0;
        return NULL;
    }
    else
        return FUNC_PTRYBLOCK(*pFuncInfo, *pStart, pDC->ImageBase);
}

extern "C" _CRTIMP FRAMEINFO * __cdecl _CreateFrameInfo(
    FRAMEINFO * pFrameInfo,
    PVOID       pExceptionObject
) {
    pFrameInfo->pExceptionObject = pExceptionObject;
    pFrameInfo->pNext            = (pFrameInfo < pFrameInfoChain)? pFrameInfoChain : NULL;
    pFrameInfoChain              = pFrameInfo;
    return pFrameInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
// _IsExceptionObjectToBeDestroyed - Determine if an exception object is still
//  in use by a more deeply nested catch frame, or if it unused and should be
//  destroyed on exiting from the current catch block.
//
// Returns:
//      TRUE if exception object not found and should be destroyed.
//
extern "C" _CRTIMP BOOL __cdecl _IsExceptionObjectToBeDestroyed(
    PVOID pExceptionObject
) {
    FRAMEINFO * pFrameInfo;

    for (pFrameInfo = pFrameInfoChain; pFrameInfo != NULL; pFrameInfo = pFrameInfo->pNext ) {
        if( pFrameInfo->pExceptionObject == pExceptionObject ) {
            return FALSE;
        }
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
//
// _FindAndUnlinkFrame - Pop the frame information for this scope that was
//  pushed by _CreateFrameInfo.  This should be the first frame in the list,
//  but the code will look for a nested frame and pop all frames, just in
//  case.
//
extern "C" _CRTIMP void __cdecl _FindAndUnlinkFrame(
    FRAMEINFO * pFrameInfo
) {
    DASSERT(pFrameInfo == pFrameInfoChain);

    for (FRAMEINFO *pCurFrameInfo = pFrameInfoChain;
         pCurFrameInfo != NULL;
         pCurFrameInfo = pCurFrameInfo->pNext)
    {
        if (pFrameInfo == pCurFrameInfo) {
            pFrameInfoChain = pCurFrameInfo->pNext;
            return;
        }
    }

    // Should never be reached.
    DASSERT(0);
}


extern "C" void _UnwindNestedFrames(
    EHRegistrationNode  *pFrame,            // Unwind up to (but not including) this frame
    EHExceptionRecord   *pExcept,           // The exception that initiated this unwind
    CONTEXT             *pContext,           // Context info for current exception
    EHRegistrationNode  *pEstablisher,
    void                *Handler,
    __ehstate_t         TargetUnwindState,
    FuncInfo            *pFuncInfo,
    DispatcherContext   *pDC,
    BOOLEAN             recursive

) {
    static const EXCEPTION_RECORD ExceptionTemplate = { // A generic exception record
        0x80000029L,                    // STATUS_UNWIND_CONSOLIDATE
        EXCEPTION_NONCONTINUABLE,       // Exception flags (we don't do resume)
        NULL,                           // Additional record (none)
        NULL,                           // Address of exception (OS fills in)
        15,        // Number of parameters
        {   EH_MAGIC_NUMBER1,           // Our version control magic number
            NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL
        }                      // pThrowInfo
    };
    CONTEXT Context;
    EXCEPTION_RECORD ExceptionRecord = ExceptionTemplate;
    ExceptionRecord.ExceptionInformation[0] = (ULONG_PTR)__CxxCallCatchBlock;
                // Address of call back function
    ExceptionRecord.ExceptionInformation[1] = (ULONG_PTR)pEstablisher;
                // Used by callback funciton
    ExceptionRecord.ExceptionInformation[2] = (ULONG_PTR)Handler;
                // Used by callback function to call catch block
    ExceptionRecord.ExceptionInformation[3] = (ULONG_PTR)TargetUnwindState;
                // Used by CxxFrameHandler to unwind to target_state
    ExceptionRecord.ExceptionInformation[4] = (ULONG_PTR)pContext;
                // used to set pCurrentExContext in callback function
    ExceptionRecord.ExceptionInformation[5] = (ULONG_PTR)pFuncInfo;
                // Used in callback function to set state on stack to -2
    ExceptionRecord.ExceptionInformation[6] = (ULONG_PTR)pExcept;
                // Used for passing current Exception
    ExceptionRecord.ExceptionInformation[7] = (ULONG_PTR)recursive;
                // Used for translated Exceptions
    ExceptionRecord.ExceptionInformation[8] = EH_MAGIC_NUMBER1;
                // Used in __InternalCxxFrameHandler to detected if it's being
                // called from _UnwindNestedFrames.
    RtlUnwindEx((void *)*pFrame,
                (void *)pDC->ControlPc,    // Address where control left function
                &ExceptionRecord,
                NULL,
                &Context,
                (PUNWIND_HISTORY_TABLE)/*UNDONE: remove after SDK update*/pDC->HistoryTable);
}
