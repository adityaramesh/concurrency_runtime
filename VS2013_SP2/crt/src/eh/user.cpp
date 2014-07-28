/***
*user.cxx - E.H. functions only called by the client programs
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Exception handling functions only called by the client programs,
*       not by the C/C++ runtime itself.
*
*       Entry Points:
*       * set_terminate
*       * set_unexpected
*       * _set_seh_translator
*       * _set_inconsistency
****/

#include <stddef.h>
#include <windows.h>
#include <mtdll.h>
#include <ehassert.h>
#include <eh.h>
#include <ehhooks.h>
#include <sect_attribs.h>
#include <internal.h>
#include <stdlib.h>

#pragma hdrstop
#pragma warning(disable: 4535)

/////////////////////////////////////////////////////////////////////////////
//
// set_terminate - install a new terminate handler (ANSI Draft 17.1.2.1.3)
//

_CRTIMP terminate_function __cdecl
set_terminate( terminate_function pNew )
{
    terminate_function pOld = NULL;

#if defined(_DEBUG)

#pragma warning(disable:4191)

    if ( (pNew == NULL) || _ValidateExecute( (FARPROC) pNew ) )

#pragma warning(default:4191)

#endif
    {
        pOld = __pTerminate == NULL
            ? &abort : __pTerminate; // can't return NULL
        __pTerminate = pNew;
    }

    return pOld;
}

extern "C" _CRTIMP terminate_function __cdecl
_get_terminate( void )
{
    return __pTerminate;
}


/////////////////////////////////////////////////////////////////////////////
//
// set_unexpected - install a new unexpected handler (ANSI Draft 17.1.2.1.3)
//

_CRTIMP unexpected_function __cdecl
set_unexpected( unexpected_function pNew )
{
    unexpected_function pOld = NULL;

#if defined(_DEBUG)

#pragma warning(disable:4191)

    if ( (pNew == NULL) || _ValidateExecute( (FARPROC) pNew ) )

#pragma warning(default:4191)

#endif
    {
        pOld = __pUnexpected == NULL
            ? &terminate : __pUnexpected; // can't return NULL
        __pUnexpected = pNew;
    }

    return pOld;
}


extern "C" _CRTIMP unexpected_function __cdecl
_get_unexpected( )
{
    return __pUnexpected;
}


/////////////////////////////////////////////////////////////////////////////
//
// _set_se_translator - install a new SE to C++ EH translator.
//
// The 'new' seh translator may be NULL, because the default one is.
//

_CRTIMP _se_translator_function __cdecl
_set_se_translator( _se_translator_function pNew )
{
    _se_translator_function pOld = NULL;

#ifdef _DEBUG

#pragma warning(disable:4191)

    if ( (pNew == NULL) || _ValidateExecute( (FARPROC)pNew ) )

#pragma warning(default:4191)

#endif
    {
        pOld = __pSETranslator;
        __pSETranslator = pNew;
    }

    return pOld;
}

/////////////////////////////////////////////////////////////////////////////
//
// _set_inconsistency - install a new inconsistency handler(Internal Error)
//
// (This function is currently not documented for end-users.  At some point,
//  it might be advantageous to allow end-users to "catch" internal errors
//  from the EH CRT, but for now, they will terminate(). )

_inconsistency_function __cdecl
__set_inconsistency( _inconsistency_function pNew)
{
    _inconsistency_function pOld = NULL;

#if defined(_DEBUG)

#pragma warning(disable:4191)

    if ( (pNew == NULL) || _ValidateExecute( (FARPROC)pNew ) )

#pragma warning(default:4191)

#endif
    {
        pOld = (_inconsistency_function) DecodePointer(__pInconsistency);
        __pInconsistency = (_inconsistency_function) EncodePointer(pNew);
    }

    return pOld;
}

_CRTIMP terminate_function set_terminate(int _I)
{
    if (_I == 0)
    {
        return set_terminate((terminate_function)0);
    }
    return 0; /* NULL */
}

_CRTIMP unexpected_function set_unexpected(int _I)
{
    if (_I == 0)
    {
        return set_unexpected((unexpected_function)0);
    }
    return 0; /* NULL */
}

_CRTIMP _se_translator_function _set_se_translator(int _I)
{
    if (_I == 0)
    {
        return _set_se_translator((_se_translator_function)0);
    }
    return 0; /* NULL */
}
