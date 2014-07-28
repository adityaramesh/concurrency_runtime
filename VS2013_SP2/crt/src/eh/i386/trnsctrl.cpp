/***
*trnsctrl.cxx -  Routines for doing control transfers
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Routines for doing control transfers; written using inline
*       assembly in naked functions.  Contains the public routine
*       _CxxFrameHandler, the entry point for the frame handler
****/

#include <windows.h>

#include <mtdll.h>

#include <ehdata.h>
#include <trnsctrl.h>
#include <eh.h>
#include <ehhooks.h>
#include <ehassert.h>
#include <process.h>
#include <internal.h>

// copied from ntxcapi.h
#define EXCEPTION_UNWINDING 0x2         // Unwind is in progress
#define EXCEPTION_EXIT_UNWIND 0x4       // Exit unwind is in progress
#define EXCEPTION_STACK_INVALID 0x8     // Stack out of limits or unaligned
#define EXCEPTION_NESTED_CALL 0x10      // Nested exception handler call
#define EXCEPTION_TARGET_UNWIND 0x20    // Target unwind in progress
#define EXCEPTION_COLLIDED_UNWIND 0x40  // Collided exception handler call

#define EXCEPTION_UNWIND (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | \
                          EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)

#define IS_UNWINDING(Flag) ((Flag & EXCEPTION_UNWIND) != 0)
#define IS_DISPATCHING(Flag) ((Flag & EXCEPTION_UNWIND) == 0)
#define IS_TARGET_UNWIND(Flag) (Flag & EXCEPTION_TARGET_UNWIND)



#pragma hdrstop

#include <setjmp.h>

#pragma warning(disable:4311 4312)      // x86 specific, ignore /Wp64 warnings
#pragma warning(disable:4731)           // ignore EBP mod in inline-asm warning
#pragma warning(disable:4733)           // ignore unsafe FS:0 modifications

#define pFrameInfoChain   (*((FRAMEINFO **)    &(_getptd()->_pFrameInfoChain)))

/////////////////////////////////////////////////////////////////////////////
//
// _JumpToContinuation - sets up EBP and jumps to specified code address.
//
// Does not return.
//
// NT leaves a marker registration node at the head of the list, under the
// assumption that RtlUnwind will remove it.  As it happens, we need to keep
// it in case of a rethrow (see below).  We only remove the current head
// (assuming it is NT's), because there may be other nodes that we still
// need.
//

void __stdcall _JumpToContinuation(
    void                *target,    // The funclet to call
    EHRegistrationNode  *pRN        // Registration node, represents location of frame
) {
    EHTRACE_ENTER_FMT1("Transfer to 0x%p", target);
    EHTRACE_RESET;

    register long targetEBP;

#if !CC_EXPLICITFRAME
    targetEBP = (long)pRN + FRAME_OFFSET;
#else
    targetEBP = pRN->frame;
#endif

    __asm {
        //
        // Unlink NT's marker node:
        //
        mov     ebx, FS:[0]
        mov     eax, [ebx]
        mov     FS:[0], eax

        //
        // Transfer control to the continuation point
        //
        mov     eax, target         // Load target address
        mov     ebx, pRN
        mov     ebp, targetEBP      // Load target frame pointer
        mov     esp, [ebx-4]        // Restore target esp
        jmp     eax                 // Call the funclet
        }
    }


/////////////////////////////////////////////////////////////////////////////
//
// _CallMemberFunction0 - call a parameterless member function using __thiscall
//                       calling convention, with 0 parameters.
//

__declspec(naked) void __stdcall _CallMemberFunction0(
    void *pthis,        // Value for 'this' pointer
    void *pmfn          // Pointer to the member function
) {
    __asm {
        pop     eax         // Save return address
        pop     ecx         // Get 'this'
        xchg    [esp],eax   // Get function address, stash return address
        jmp     eax         // jump to the function (function will return
                            // to caller of this func)
        }
    }


/////////////////////////////////////////////////////////////////////////////
//
// _CallMemberFunction1 - call a member function using __thiscall
//                       calling convention, with 1 parameter.
//

__declspec(naked) void __stdcall _CallMemberFunction1(
    void *pthis,        // Value for 'this' pointer
    void *pmfn,         // Pointer to the member function
    void *pthat         // Value of 1st parameter (type assumes copy ctor)
) {
    __asm {
        pop     eax         // Save return address
        pop     ecx         // Get 'this'
        xchg    [esp],eax   // Get function address, stash return address
        jmp     eax         // jump to the function (function will return
                            // to caller of this func)
        }
    }


/////////////////////////////////////////////////////////////////////////////
//
// _CallMemberFunction2 - call a member function using __thiscall
//                       calling convention, with 2 parameter.
//

__declspec(naked) void __stdcall _CallMemberFunction2(
    void *pthis,        // Value for 'this' pointer
    void *pmfn,         // Pointer to the member function
    void *pthat,        // Value of 1st parameter (type assumes copy ctor)
    int   val2          // Value of 2nd parameter (type assumes copy ctor w/vb)
) {
    __asm {
        pop     eax         // Save return address
        pop     ecx         // Get 'this'
        xchg    [esp],eax   // Get function address, stash return address
        jmp     eax         // jump to the function (function will return
                            // to caller of this func)
        }
    }


/////////////////////////////////////////////////////////////////////////////
//
// _UnwindNestedFrames - Call RtlUnwind, passing the address after the call
//                      as the continuation address.
//
//  Win32 assumes that after a frame has called RtlUnwind, it will never return
// to the dispatcher.
//
// Let me explain:
//  When the dispatcher calls a frame handler while searching
// for the appropriate handler, it pushes an extra guard registration node
// onto the list.  When the handler returns to the dispatcher, the dispatcher
// assumes that its node is at the head of the list, restores esp from the
// address of the head node, and then unlinks that node from the chain.
//  However, if RtlUnwind removes ALL nodes below the specified node, including
// the dispatcher's node, so without intervention the result is that the
// current subject node gets poped from the list, and the stack pointer gets
// reset to somewhere within the frame of that node, which is totally bogus
// (this last side effect is not a problem, because esp is then immediately
// restored from the ebp chain, which is still valid).
//
// So:
//  To get arround this, WE ASSUME that the registration node at the head of
// the list is the dispatcher's marker node (which it is in NT 1.0), and
// we keep a handle to it when we call RtlUnwind, and then link it back in
// after RtlUnwind has done its stuff.  That way, the dispatcher restores
// its stack exactly as it expected to, and leave our registration node alone.
//
// What happens if there is an exception during the unwind?
// We can't put a registration node here, because it will be removed
// immediately.
//
// RtlUnwind:
//  RtlUnwind is evil.  It trashes all the registers except EBP and ESP.
// Because of that, EBX, ESI, and EDI must be preserved by this function,
// and the compiler may not assume that any callee-save register can be used
// across the call to RtlUnwind.  To accomplish the former, inline-asm code
// here uses EBX, ESI, and EDI, so they will be saved in the prologue.  For
// the latter, optimizations are disabled for the duration of this function.
//

#pragma optimize("g", off)      // WORKAROUND for DOLPH:3322

void __stdcall _UnwindNestedFrames(
    EHRegistrationNode *pRN,        // Unwind up to (but not including) this frame
    EHExceptionRecord   *pExcept    // The exception that initiated this unwind
) {
    EHTRACE_ENTER;

    void* pReturnPoint;
    EHRegistrationNode *pDispatcherRN;  // Magic!

    __asm {
        //
        // Save the dispatcher's marker node
        //
        // NOTE: RtlUnwind will trash the callee-save regs EBX, ESI, and EDI.
        // We explicitly use them here in the inline-asm so they get preserved
        // and restored by the function prologue/epilogue.
        //
        mov     esi, dword ptr FS:[0]   // use ESI
        mov     pDispatcherRN, esi
    }

    __asm mov pReturnPoint, offset ReturnPoint
    RtlUnwind(pRN, pReturnPoint, (PEXCEPTION_RECORD)pExcept, NULL);

ReturnPoint:

    PER_FLAGS(pExcept) &= ~EXCEPTION_UNWINDING; // Clear the 'Unwinding' flag
                                                // in case exception is rethrown
    __asm {
        //
        // Re-link the dispatcher's marker node
        //
        mov     edi, dword ptr FS:[0]   // Get the current head (use EDI)
        mov     ebx, pDispatcherRN      // Get the saved head (use EBX)
        mov     [ebx], edi              // Link saved head to current head
        mov     dword ptr FS:[0], ebx   // Make saved head current head
        }

    EHTRACE_EXIT;

    return;
    }

#pragma optimize("", on)

/////////////////////////////////////////////////////////////////////////////
//
// __CxxFrameHandler - Real entry point to the runtime; this thunk fixes up
//      the parameters, and then calls the workhorse.
//
extern "C" EXCEPTION_DISPOSITION __cdecl __InternalCxxFrameHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    void               *pContext,       // Context info (we don't care what's in it)
    DispatcherContext  *pDC,            // More dynamic info for this frame (ignored on Intel)
    FuncInfo           *pFuncInfo,      // Static information for this frame
    int                 CatchDepth,     // How deeply nested are we?
    EHRegistrationNode *pMarkerRN,      // Marker node for when checking inside
                                        //  catch block
    BOOL                recursive);     // True if this is a translation exception

//
// This is a backwards-compatibility entry point. All new code must go to __CxxFrameHandler2
//
extern "C" _CRTIMP __declspec(naked) EXCEPTION_DISPOSITION __cdecl __CxxFrameHandler(
/*
    EAX=FuncInfo   *pFuncInfo,          // Static information for this frame
*/
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    void               *pContext,       // Context info (we don't care what's in it)
    DispatcherContext  *pDC             // More dynamic info for this frame (ignored on Intel)
) {
    FuncInfo   *pFuncInfo;
    EXCEPTION_DISPOSITION result;

    __asm {
        //
        // Standard function prolog
        //
        push    ebp
        mov     ebp, esp
        sub     esp, __LOCAL_SIZE
        push    ebx
        push    esi
        push    edi
        cld             // A bit of paranoia -- Our code-gen assumes this

        //
        // Save the extra parameter
        //
        mov     pFuncInfo, eax
        }

    EHTRACE_ENTER_FMT1("pRN = 0x%p", pRN);

    result = __InternalCxxFrameHandler( pExcept, pRN, pContext, pDC, pFuncInfo, 0, NULL, FALSE );

    EHTRACE_HANDLER_EXIT(result);

    __asm {
        pop     edi
        pop     esi
        pop     ebx
        mov     eax, result
        mov     esp, ebp
        pop     ebp
        ret     0
        }
}

//
// __CxxFrameHandler3 - Real entry point to the runtime
//                                              __CxxFrameHandler2 is an alias for __CxxFrameHandler3
//                                              since they are compatible in VC version of CRT
//                      These function should be separated out if a change makes
//                                              __CxxFrameHandler3 incompatible with __CxxFrameHandler2
//
extern "C" _CRTIMP __declspec(naked) EXCEPTION_DISPOSITION __cdecl __CxxFrameHandler3(
/*
    EAX=FuncInfo   *pFuncInfo,          // Static information for this frame
*/
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    void               *pContext,       // Context info (we don't care what's in it)
    DispatcherContext  *pDC             // More dynamic info for this frame (ignored on Intel)
) {
    FuncInfo   *pFuncInfo;
    EXCEPTION_DISPOSITION result;

    __asm {
        //
        // Standard function prolog
        //
        push    ebp
        mov     ebp, esp
        sub     esp, __LOCAL_SIZE
        push    ebx
        push    esi
        push    edi
        cld             // A bit of paranoia -- Our code-gen assumes this

        //
        // Save the extra parameter
        //
        mov     pFuncInfo, eax
        }

    EHTRACE_ENTER_FMT1("pRN = 0x%p", pRN);

    result = __InternalCxxFrameHandler( pExcept, pRN, pContext, pDC, pFuncInfo, 0, NULL, FALSE );

    EHTRACE_HANDLER_EXIT(result);

    __asm {
        pop     edi
        pop     esi
        pop     ebx
        mov     eax, result
        mov     esp, ebp
        pop     ebp
        ret     0
        }
}

//
// __CxxFrameHandler2 - Remove after compiler is updated
//
extern "C" _CRTIMP __declspec(naked) EXCEPTION_DISPOSITION __cdecl __CxxFrameHandler2(
/*
    EAX=FuncInfo   *pFuncInfo,          // Static information for this frame
*/
    EHExceptionRecord  *pExcept,        // Information for this exception
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    void               *pContext,       // Context info (we don't care what's in it)
    DispatcherContext  *pDC             // More dynamic info for this frame (ignored on Intel)
) {
    FuncInfo   *pFuncInfo;
    EXCEPTION_DISPOSITION result;

    __asm {
        //
        // Standard function prolog
        //
        push    ebp
        mov     ebp, esp
        sub     esp, __LOCAL_SIZE
        push    ebx
        push    esi
        push    edi
        cld             // A bit of paranoia -- Our code-gen assumes this

        //
        // Save the extra parameter
        //
        mov     pFuncInfo, eax
        }

    EHTRACE_ENTER_FMT1("pRN = 0x%p", pRN);

    result = __InternalCxxFrameHandler( pExcept, pRN, pContext, pDC, pFuncInfo, 0, NULL, FALSE );

    EHTRACE_HANDLER_EXIT(result);

    __asm {
        pop     edi
        pop     esi
        pop     ebx
        mov     eax, result
        mov     esp, ebp
        pop     ebp
        ret     0
        }
}

/////////////////////////////////////////////////////////////////////////////
//
// __CxxLongjmpUnwind - Entry point for local unwind required by longjmp
//      when setjmp used in same function as C++ EH.
//
extern "C" void __FrameUnwindToState(   // in frame.cpp
    EHRegistrationNode *pRN,            // Dynamic information for this frame
    DispatcherContext  *pDC,            // More dynamic info for this frame (ignored on Intel)
    FuncInfo           *pFuncInfo,      // Static information for this frame
    __ehstate_t         targetState);   // State to unwind to

extern "C" void __stdcall __CxxLongjmpUnwind(
    _JUMP_BUFFER       *jbuf
) {
    EHTRACE_ENTER;

    __FrameUnwindToState((EHRegistrationNode *)jbuf->Registration,
                         (DispatcherContext*)NULL,
                         (FuncInfo *)jbuf->UnwindData[0],
                         (__ehstate_t)jbuf->TryLevel);

    EHTRACE_EXIT;
}

/////////////////////////////////////////////////////////////////////////////
//
// _CallCatchBlock2 - The nitty-gritty details to get the catch called
//      correctly.
//
// We need to guard the call to the catch block with a special registration
// node, so that if there is an exception which should be handled by a try
// block within the catch, we handle it without unwinding the SEH node
// in CallCatchBlock.
//

struct CatchGuardRN {
    EHRegistrationNode *pNext;          // Frame link
    void               *pFrameHandler;  // Frame Handler
    UINT_PTR            RandomCookie;   // __security_cookie XOR node address
    FuncInfo           *pFuncInfo;      // Static info for subject function
    EHRegistrationNode *pRN;            // Dynamic info for subject function
    int                 CatchDepth;     // How deeply nested are we?
#if defined(ENABLE_EHTRACE)
    int                 trace_level;    // Trace level to restore in handler
#endif
    };

static EXCEPTION_DISPOSITION __cdecl CatchGuardHandler( EHExceptionRecord*, CatchGuardRN *, void *, void * );

void *_CallCatchBlock2(
    EHRegistrationNode *pRN,            // Dynamic info of function with catch
    FuncInfo           *pFuncInfo,      // Static info of function with catch
    void               *handlerAddress, // Code address of handler
    int                CatchDepth,      // How deeply nested in catch blocks are we?
    unsigned long      NLGCode
) {
    EHTRACE_ENTER;

    //
    // First, create and link in our special guard node:
    //
    CatchGuardRN CGRN = { NULL,
                          (void*)CatchGuardHandler,
                          __security_cookie ^ (UINT_PTR)&CGRN,
                          pFuncInfo,
                          pRN,
                          CatchDepth + 1
#if defined(ENABLE_EHTRACE)
                          , __ehtrace_level
#endif
    };

    __asm {
        mov     eax, FS:[0]     // Fetch frame list head
        mov     CGRN.pNext, eax // Link this node in
        lea     eax, CGRN       // Put this node at the head
        mov     FS:[0], eax
        }

    //
    // Call the catch
    //
    void *continuationAddress = _CallSettingFrame( handlerAddress, pRN, NLGCode );

    //
    // Unlink our registration node
    //
    __asm {
        mov     eax, CGRN.pNext // Get parent node
        mov     FS:[0], eax     // Put it at the head
        }

    EHTRACE_EXIT;

    return continuationAddress;
    }


/////////////////////////////////////////////////////////////////////////////
//
// CatchGuardHandler - frame handler for the catch guard node.
//
// This function will attempt to find a handler for the exception within
// the current catch block (ie any nested try blocks).  If none is found,
// or the handler rethrows, returns ExceptionContinueSearch; otherwise does
// not return.
//
// Does nothing on an unwind.
//

static EXCEPTION_DISPOSITION __cdecl CatchGuardHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    CatchGuardRN       *pRN,            // The special marker frame
    void               *pContext,       // Context info (we don't care what's in it)
    void *                              // (ignored)
) {
#if defined(ENABLE_EHTRACE)
    EHTracePushLevel(pRN->trace_level);
#endif
    EHTRACE_ENTER_FMT1("pRN = 0x%p", pRN);

    __asm cld;      // Our code-gen assumes this

    //
    // Validate our registration record, to secure against hacker attacks.
    //

    __security_check_cookie(pRN->RandomCookie ^ (UINT_PTR)pRN);

    EXCEPTION_DISPOSITION result =
        __InternalCxxFrameHandler( pExcept,
                                   pRN->pRN,
                                   pContext,
                                   NULL,
                                   pRN->pFuncInfo,
                                   pRN->CatchDepth,
                                   (EHRegistrationNode*)pRN,
                                   FALSE );

    EHTRACE_HANDLER_EXIT(result);
    EHTRACE_RESTORE_LEVEL(true);
    return result;
    }


/////////////////////////////////////////////////////////////////////////////
//
// CallSEHTranslator - calls the SEH translator, and handles the translation
//      exception.
//
// Assumes that a valid translator exists.
//
// Method:
//  Sets up a special guard node, whose handler handles the translation
// exception, and remembers NT's marker node (See _UnwindNestedFrames above).
// If the exception is not fully handled, the handler returns control to here,
// so that this function can return to resume the normal search for a handler
// for the original exception.
//
// Returns: TRUE if translator had a translation (handled or not)
//          FALSE if there was no translation
//          Does not return if translation was fully handled
//
// Note:
//     This is also called in a special mode from TranslatorGuardHandler
// to return the address of the continuation point, ExceptionContinuation,
// a label inside CallSEHTranslator.  We used to keep this address in the
// TranslatorGuardRN, but that opens a security hole by allowing a buffer
// overrun exploit to overwrite an EH registration record and fill in the
// continuation point to vector wherever desired.
//
//     The special mode is detected by a first argument having the value 0x123.
// That is never a legitimate pointer and unambiguously indicates a special
// case call.  The 2nd arg in this case is treated as a void** to be used to
// return the continuation address.
//

struct TranslatorGuardRN /*: CatchGuardRN */ {
    EHRegistrationNode *pNext;          // Frame link
    void               *pFrameHandler;  // Frame Handler
    UINT_PTR            RandomCookie;   // __security_cookie XOR node address
    FuncInfo           *pFuncInfo;      // Static info for subject function
    EHRegistrationNode *pRN;            // Dynamic info for subject function
    int                 CatchDepth;     // How deeply nested are we?
    EHRegistrationNode *pMarkerRN;      // Marker for parent context
    void               *ESP;            // ESP within CallSEHTranslator
    void               *EBP;            // EBP within CallSEHTranslator
    BOOL                DidUnwind;      // True if this frame was unwound
#if defined(ENABLE_EHTRACE)
    int                 trace_level;    // Trace level to restore in handler
#endif
    };

static EXCEPTION_DISPOSITION __cdecl TranslatorGuardHandler( EHExceptionRecord*, TranslatorGuardRN *, void *, void * );

#define CSET_SPECIAL ((EHExceptionRecord *)0x123)

#pragma optimize("g", off)              // WORKAROUND for DOLPH:3322

BOOL _CallSETranslator(
    EHExceptionRecord  *pExcept,        // The exception to be translated
    EHRegistrationNode *pRN,            // Dynamic info of function with catch
    void               *pContext,       // Context info (we don't care what's in it)
    DispatcherContext  *pDC,            // More dynamic info of function with catch (ignored)
    FuncInfo           *pFuncInfo,      // Static info of function with catch
    int                 CatchDepth,     // How deeply nested in catch blocks are we?
    EHRegistrationNode *pMarkerRN       // Marker for parent context
) {
    //
    // Process special case calling request - return address of internal
    // continuation label through pRN (which is actually a void** in this case)
    //
    if (pExcept == CSET_SPECIAL) {
        __asm {
            mov     eax, offset ExceptionContinuation
            mov     ecx, pRN
            mov     [ecx], eax
        }
        return TRUE;
    }

    EHTRACE_ENTER;

    //
    // Create and link in our special guard node:
    //
    TranslatorGuardRN TGRN = {  NULL,       // Frame link
                                (void*)TranslatorGuardHandler,
                                __security_cookie ^ (UINT_PTR)&TGRN,
                                pFuncInfo,
                                pRN,
                                CatchDepth,
                                pMarkerRN,
                                NULL,       // ESP
                                NULL,       // EBP
                                FALSE       // DidUnwind
#if defined(ENABLE_EHTRACE)
                                , __ehtrace_level
#endif
    };

    __asm {
        //
        // Fill in the blanks:
        //
        mov     TGRN.ESP, esp
        mov     TGRN.EBP, ebp

        //
        // Link this node in:
        //
        mov     eax, FS:[0]             // Fetch frame list head
        mov     TGRN.pNext, eax         // Link this node in
        lea     eax, TGRN               // Put this node at the head
        mov     FS:[0], eax
        }

    //
    // Call the translator; assume it will give a translation.
    //
    BOOL DidTranslate = TRUE;
    _EXCEPTION_POINTERS pointers = {
        (PEXCEPTION_RECORD)pExcept,
        (PCONTEXT)pContext };

    _se_translator_function pSETranslator;
    pSETranslator = __pSETranslator;
    pSETranslator(PER_CODE(pExcept), &pointers);

    //
    // If translator returned normally, that means it didn't translate the
    // exception.
    //
    DidTranslate = FALSE;

    //
    // Here's where we pick up if the translator threw something.
    // Note that ESP and EBP were restored by our frame handler.
    //
ExceptionContinuation:

    if (TGRN.DidUnwind) {
        //
        // If the translated exception was partially handled (ie caught but
        // rethrown), then the frame list has the NT guard for the translation
        // exception context instead of the one for the original exception
        // context.  Correct that sequencing problem.  Note that our guard
        // node was unlinked by RtlUnwind.
        //
        __asm {
            mov     ebx, FS:[0]     // Get the node below the (bad) NT marker
            mov     eax, [ebx]      //  (it was the target of the unwind)
            mov     ebx, TGRN.pNext // Get the node we saved (the 'good' marker)
            mov     [ebx], eax      // Link the good node to the unwind target
            mov     FS:[0], ebx     // Put the good node at the head of the list
            }
        }
    else {
        //
        // Translator returned normally or translation wasn't handled.
        // unlink our registration node and exit
        //
        __asm {
            mov     eax, TGRN.pNext // Get parent node
            mov     FS:[0], eax     // Put it at the head
            }
        }

    EHTRACE_EXIT;

    return DidTranslate;
    }

#pragma optimize("g", on)


/////////////////////////////////////////////////////////////////////////////
//
// TranslatorGuardHandler - frame handler for the translator guard node.
//
// On search:
//  This frame handler will check if there is a catch at the current level
//  for the translated exception.  If there is no handler or the handler
//  did a re-throw, control is transfered back into CallSEHTranslator, based
//  on the values saved in the registration node.
//
//  Does not return.
//
// On unwind:
//  Sets the DidUnwind flag in the registration node, and returns.
//
static EXCEPTION_DISPOSITION __cdecl TranslatorGuardHandler(
    EHExceptionRecord  *pExcept,        // Information for this exception
    TranslatorGuardRN  *pRN,            // The translator guard frame
    void               *pContext,       // Context info (we don't care what's in it)
    void *                              // (ignored)
) {
#if defined(ENABLE_EHTRACE)
    EHTracePushLevel(pRN->trace_level);
#endif
    EHTRACE_ENTER_FMT1("pRN = 0x%p", pRN);

    __asm cld;      // Our code-gen assumes this

    //
    // Validate our registration record, to secure against hacker attacks.
    //

    __security_check_cookie(pRN->RandomCookie ^ (UINT_PTR)pRN);

    if (IS_UNWINDING(PER_FLAGS(pExcept)))
        {
        pRN->DidUnwind = TRUE;

        EHTRACE_HANDLER_EXIT(ExceptionContinueSearch);
        EHTRACE_RESTORE_LEVEL(true);
        return ExceptionContinueSearch;
        }
    else {
        //
        // Check for a handler:
        //
        __InternalCxxFrameHandler( pExcept, pRN->pRN, pContext, NULL, pRN->pFuncInfo, pRN->CatchDepth, pRN->pMarkerRN, TRUE );

        if (!pRN->DidUnwind) {
            //
            // If no match was found, unwind the context of the translator
            //
            _UnwindNestedFrames( (EHRegistrationNode*)pRN, pExcept );
            }

        //
        // Transfer control back to establisher:
        //

        void *pContinue;
        _CallSETranslator(CSET_SPECIAL, (EHRegistrationNode *)&pContinue,
                          NULL, NULL, NULL, 0, NULL);

        EHTRACE_FMT1("Transfer to establisher @ 0x%p", pContinue);
        EHTRACE_RESTORE_LEVEL(false);
        EHTRACE_EXIT;

        __asm {
            mov     eax, pContinue
            mov     ebx, pRN    // Get address of registration node
            mov     esp, [ebx]TranslatorGuardRN.ESP
            mov     ebp, [ebx]TranslatorGuardRN.EBP
            jmp     eax
            }

        // Unreached.
        return ExceptionContinueSearch;
        }
    }


/////////////////////////////////////////////////////////////////////////////
//
// _GetRangeOfTrysToCheck - determine which try blocks are of interest, given
//   the current catch block nesting depth.  We only check the trys at a single
//   depth.
//
// Returns:
//      Address of first try block of interest is returned
//      pStart and pEnd get the indices of the range in question
//

TryBlockMapEntry* _GetRangeOfTrysToCheck(
        FuncInfo   *pFuncInfo,
        int                     CatchDepth,
        __ehstate_t curState,
        unsigned   *pStart,
        unsigned   *pEnd
) {
        TryBlockMapEntry *pEntry = FUNC_PTRYBLOCK(*pFuncInfo, 0);
        unsigned start = FUNC_NTRYBLOCKS(*pFuncInfo);
        unsigned end = start;
        unsigned end1 = end;

        while (CatchDepth >= 0) {
                DASSERT(start != -1);
                start--;
                if ( TBME_HIGH(pEntry[start]) < curState && curState <= TBME_CATCHHIGH(pEntry[start])
                        || (start == -1)
                ) {
                        CatchDepth--;
                        end = end1;
                        end1 = start;
                        }
                }

        *pStart = ++start;              // We always overshoot by 1 (we may even wrap around)
        *pEnd = end;

        DASSERT( end <= FUNC_NTRYBLOCKS(*pFuncInfo) && start <= end );

        return &(pEntry[start]);
        }


/////////////////////////////////////////////////////////////////////////////
//
// _CreateFrameInfo - Save the frame information for this scope just before
//  calling the catch block.  Put it at the head of the linked list.  For
//  x86, all we need to save is the pointer to the exception object, so we
//  can determine when that object is no longer used by any nested catch
//  handler and can thus be destroyed on exiting from a catch.
//
// Returns:
//      Pointer to the frame info (the first input argument).
//
extern "C" _CRTIMP FRAMEINFO * __cdecl _CreateFrameInfo(
    FRAMEINFO * pFrameInfo,
    PVOID       pExceptionObject
) {
    pFrameInfo->pExceptionObject = pExceptionObject;
    pFrameInfo->pNext            = pFrameInfoChain;
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
// _FindAndUnlinkFrame - Remove the frame information for this scope that was
//  inserted by _CreateFrameInfo.  This should be the first frame in the list
//  (Ideally), but fibers deviate from ideal situation.
//
extern "C" _CRTIMP void __cdecl _FindAndUnlinkFrame(
    FRAMEINFO * pFrameInfo
) {
    if (pFrameInfo == pFrameInfoChain) {
        pFrameInfoChain = pFrameInfo->pNext;
        return;
    } else {
        for (FRAMEINFO *pCurFrameInfo = pFrameInfoChain;
             pCurFrameInfo->pNext != NULL;
             pCurFrameInfo = pCurFrameInfo->pNext)
        {
            if (pFrameInfo == pCurFrameInfo->pNext) {
                pCurFrameInfo->pNext = pFrameInfo->pNext;
                return;
            }
        }
    }

    // Should never be reached.
    DASSERT(0);
}
