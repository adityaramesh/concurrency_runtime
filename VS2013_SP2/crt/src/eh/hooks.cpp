/***
*hooks.cxx - global (per-thread) variables and functions for EH callbacks
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       global (per-thread) variables for assorted callbacks, and
*       the functions that do those callbacks.
*
*       Entry Points:
*
*       * terminate()
*       * unexpected()
*       * _inconsistency()
*
*       External Names: (only for single-threaded version)
*
*       * __pSETranslator
*       * __pTerminate
*       * __pUnexpected
*       * __pInconsistency
****/

#include <stddef.h>
#include <stdlib.h>
#include <excpt.h>

#include <windows.h>
#include <mtdll.h>
#include <eh.h>
#include <ehhooks.h>
#include <ehassert.h>
#include <internal.h>

#pragma hdrstop

/////////////////////////////////////////////////////////////////////////////
//
// The global variables:
//

_inconsistency_function __pInconsistency= 0; /* not a valid state */

/////////////////////////////////////////////////////////////////////////////
//
// _initp_eh_hooks - initialize global pointer defined in this module.
//

extern "C"
void __cdecl _initp_eh_hooks(void* enull)
{
    __pInconsistency = (_inconsistency_function) EncodePointer(&terminate);
}

/////////////////////////////////////////////////////////////////////////////
//
// terminate - call the terminate handler (presumably we went south).
//              THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

_CRTIMP __declspec(noreturn) void __cdecl terminate(void)
{
        EHTRACE_ENTER_MSG("No exit");

        {
            terminate_function pTerminate;
            pTerminate = __pTerminate;
            if ( pTerminate != NULL ) {
                /*
                Note: If the user's terminate handler crashes, we cannot allow an EH to propagate
                as we may be in the middle of exception processing, so we abort instead.
                */
                __try {
                    //
                    // Let the user wrap things up their way.
                    //
                    pTerminate();
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    //
                    // Intercept ANY exception from the terminate handler
                    //
                    ; // Deliberately do nothing
                }
            }
        }

        //
        // If the terminate handler returned, faulted, or otherwise failed to
        // halt the process/thread, we'll do it.
        //
        abort();
}

/////////////////////////////////////////////////////////////////////////////
//
// unexpected - call the unexpected handler (presumably we went south, or nearly).
//              THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

void __cdecl unexpected(void)
{
        unexpected_function pUnexpected;

        EHTRACE_ENTER;

        pUnexpected = __pUnexpected;
        if ( pUnexpected != NULL ) {
        //
        // Let the user wrap things up their way.
        //
            pUnexpected();
        }

        //
        // If the unexpected handler returned, we'll give the terminate handler a chance.
        //
        terminate();
}

/////////////////////////////////////////////////////////////////////////////
//
// _inconsistency - call the inconsistency handler (Run-time processing error!)
//                THIS MUST NEVER RETURN!
//
// Open issues:
//      * How do we guarantee that the whole process has stopped, and not just
//        the current thread?
//

_CRTIMP void __cdecl _inconsistency(void)
{
        _inconsistency_function pInconsistency;

        EHTRACE_ENTER;

        //
        // Let the user wrap things up their way.
        //

        pInconsistency = (_inconsistency_function) DecodePointer(__pInconsistency);
        if ( pInconsistency != NULL )
            /*
            Note: If the user's terminate handler crashes, we cannot allow an EH to propagate
            as we may be in the middle of exception processing, so we abort instead.
            */
            __try {
                pInconsistency();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                //
                // Intercept ANY exception from the terminate handler
                //
                                ; // Deliberately do nothing
            }

        //
        // If the inconsistency handler returned, faulted, or otherwise
        // failed to halt the process/thread, we'll do it.
        //
        terminate();
}
