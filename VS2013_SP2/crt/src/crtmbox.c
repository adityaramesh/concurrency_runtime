/***
*crtmbox.c - CRT MessageBoxA wrapper.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Wrap MessageBoxA.
*
*******************************************************************************/

#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <awint.h>
#include <internal.h>

/* Needed to store arguments sent to MessageBox in a Debug Packaged App */
#ifdef _UNICODE
typedef int (APIENTRY *PFNMessageBoxW)(HWND, LPCTSTR, LPCTSTR, UINT);
#define PFNMessageBox PFNMessageBoxW
typedef struct _MessageBoxArgsW
{
    PFNMessageBox dpfnMessageBox;
    LPCTSTR lpText;
    LPCTSTR lpCaption;
    UINT uType;
    INT iMsgBoxReturn;
} _MessageBoxArgsW;
#define _MessageBoxArgs _MessageBoxArgsW
#else  /* _UNICODE */
typedef int (APIENTRY *PFNMessageBoxA)(HWND, LPCTSTR, LPCTSTR, UINT);
#define PFNMessageBox PFNMessageBoxA
typedef struct _MessageBoxArgsA
{
    PFNMessageBox dpfnMessageBox;
    LPCTSTR lpText;
    LPCTSTR lpCaption;
    UINT uType;
    INT iMsgBoxReturn;
} _MessageBoxArgsA;
#define _MessageBoxArgs _MessageBoxArgsA
#endif  /* _UNICODE */

#ifdef _UNICODE
/* Create a Message box on the classic desktop for Packaged debug apps */
DWORD WINAPI __crtMessageBoxWaitThreadW(LPVOID pvArgument)
#else  /* _UNICODE */
DWORD WINAPI __crtMessageBoxWaitThreadA(LPVOID pvArgument)
#endif  /* _UNICODE */
{
    _MessageBoxArgs *args = (_MessageBoxArgs*)pvArgument;
    args->iMsgBoxReturn = (args->dpfnMessageBox)(NULL, args->lpText, args->lpCaption, args->uType);
    return 0;
}

#ifdef _UNICODE
#define __crtMessageBoxWaitThread __crtMessageBoxWaitThreadW
#define FN_MessageBox "MessageBoxW"
#define FN_GetUserObjectInformation "GetUserObjectInformationW"
#else  /* _UNICODE */
#define __crtMessageBoxWaitThread __crtMessageBoxWaitThreadA
#define FN_MessageBox "MessageBoxA"
#define FN_GetUserObjectInformation "GetUserObjectInformationA"
#endif  /* _UNICODE */


/***
*__crtMessageBox - call MessageBoxA dynamically.
*
*Purpose:
*       Avoid static link with user32.dll. Only load it when actually needed.
*
*Entry:
*       see MessageBoxA docs.
*
*Exit:
*       see MessageBoxA docs.
*
*Exceptions:
*
*******************************************************************************/
#ifdef _UNICODE
int __cdecl __crtMessageBoxW(
#else  /* _UNICODE */
int __cdecl __crtMessageBoxA(
#endif  /* _UNICODE */
        LPCTSTR lpText,
        LPCTSTR lpCaption,
        UINT uType
        )
{
        typedef int (APIENTRY *PFNMessageBox)(HWND, LPCTSTR, LPCTSTR, UINT);
        typedef HWND (APIENTRY *PFNGetActiveWindow)(void);
        typedef HWND (APIENTRY *PFNGetLastActivePopup)(HWND);
        typedef HWINSTA (APIENTRY *PFNGetProcessWindowStation)(void);
        typedef BOOL (APIENTRY *PFNGetUserObjectInformation)(HANDLE, int, PVOID, DWORD, LPDWORD);

        void *pfn = NULL;
        void *enull = EncodePointer(NULL);
        static PFNMessageBox pfnMessageBox = NULL;
        static PFNGetActiveWindow pfnGetActiveWindow = NULL;
        static PFNGetLastActivePopup pfnGetLastActivePopup = NULL;
        static PFNGetProcessWindowStation pfnGetProcessWindowStation = NULL;
        static PFNGetUserObjectInformation pfnGetUserObjectInformation = NULL;

        HWND hWndParent = NULL;
        BOOL fNonInteractive = FALSE;
        BOOL fIsPackagedApp = __crtIsPackagedApp();
        HWINSTA hwinsta=NULL;
        USEROBJECTFLAGS uof;
        DWORD nDummy;

//TODO - KERNELX
#ifndef _KERNELX
        if (NULL == pfnMessageBox)
        {
            HMODULE hlib = LoadLibraryExW(L"USER32.DLL", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hlib == NULL && GetLastError() == ERROR_INVALID_PARAMETER)
            {
                // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom,
                // try one more time using default options
                hlib = LoadLibraryExW(L"USER32.DLL", NULL, 0);
            }
            if (hlib == NULL)
                return 0;

            if (NULL == (pfn = GetProcAddress(hlib, FN_MessageBox)))
                return 0;

            pfnMessageBox = (PFNMessageBox) EncodePointer(pfn);

            pfnGetActiveWindow = (PFNGetActiveWindow)
                EncodePointer(GetProcAddress(hlib, "GetActiveWindow"));

            pfnGetLastActivePopup = (PFNGetLastActivePopup)
                EncodePointer(GetProcAddress(hlib, "GetLastActivePopup"));

            pfn = GetProcAddress(hlib, FN_GetUserObjectInformation);
            pfnGetUserObjectInformation = (PFNGetUserObjectInformation) EncodePointer(pfn);

            if (pfnGetUserObjectInformation != NULL)
                pfnGetProcessWindowStation = (PFNGetProcessWindowStation)
                EncodePointer(GetProcAddress(hlib, "GetProcessWindowStation"));
        }
#endif  /* _KERNELX */

        if (IsDebuggerPresent())
        {
            /* Output message text to the debugger.
             * This could be helpful when debugging remotely for example.
             */
            if (lpText)
                OutputDebugString(lpText);

            /* We don't wan't to display a message box for packaged apps,
              * if a debugger is already attached. Instead let the caller know
              * we want to break in the debugger
              */
            if (fIsPackagedApp)
            {
                return IDRETRY; /* retry == break in the debugger */
            }
        }

        /* In Packaged apps, display the message box in a separate thread, and wait for it */
        if (fIsPackagedApp)
        {
            PFNMessageBox dpfnMessageBox = (PFNMessageBox) DecodePointer(pfnMessageBox);
            HANDLE hThread = NULL;
            _MessageBoxArgs messageBoxArgs = {
                dpfnMessageBox,
                lpText,
                lpCaption,
                uType
            };

#ifndef _DEBUG
            /* Non-debug: Normally, we shuld not reach here except in rare instances.
               However, in retail Packaged apps, a message dialog cannot, so we abort instead.
             */
            return IDABORT; /* Abort */
#endif  /* _DEBUG */
            if(!dpfnMessageBox)
                return 0;

            if (!(hThread = CreateThread(NULL, 0, &__crtMessageBoxWaitThread, &messageBoxArgs, 0, NULL)))
                return 0;

            if (WAIT_OBJECT_0 == WaitForSingleObjectEx(hThread, INFINITE, FALSE))
                return messageBoxArgs.iMsgBoxReturn;

            return 0; /* Should never happen */
        }

        /*
         * If the current process isn't attached to a visible WindowStation,
         * (e.g. a non-interactive service), then we need to set the
         * MB_SERVICE_NOTIFICATION flag, else the message box will be
         * invisible, hanging the program.
         */

        if (pfnGetProcessWindowStation != enull && pfnGetUserObjectInformation != enull)
        {
            /* check for NULL expliticly to pacify prefix */
            PFNGetProcessWindowStation dpfnGetProcessWindowStation=(PFNGetProcessWindowStation) DecodePointer(pfnGetProcessWindowStation);
            PFNGetUserObjectInformation dpfnGetUserObjectInformation=(PFNGetUserObjectInformation) DecodePointer(pfnGetUserObjectInformation);

            if(dpfnGetProcessWindowStation && dpfnGetUserObjectInformation)
            {
                if (NULL == (hwinsta = (dpfnGetProcessWindowStation)()) ||
                    !(dpfnGetUserObjectInformation)
                    (hwinsta, UOI_FLAGS, &uof, sizeof(uof), &nDummy) ||
                    (uof.dwFlags & WSF_VISIBLE) == 0)
                {
                    fNonInteractive = TRUE;
                }
            }
        }

        if (fNonInteractive)
        {
            uType |= MB_SERVICE_NOTIFICATION;
        }
        else
        {
            if (pfnGetActiveWindow != enull)
            {
                PFNGetActiveWindow dpfnGetActiveWindow=(PFNGetActiveWindow) DecodePointer(pfnGetActiveWindow);
                if(dpfnGetActiveWindow)
                {
                    hWndParent = (dpfnGetActiveWindow)();
                }
            }

            if (hWndParent != NULL && pfnGetLastActivePopup != enull)
            {
                PFNGetLastActivePopup dpfnGetLastActivePopup=(PFNGetLastActivePopup) DecodePointer(pfnGetLastActivePopup);
                if(dpfnGetLastActivePopup)
                {
                    hWndParent = (dpfnGetLastActivePopup)(hWndParent);
                }
            }
        }

        /* scope */
        {
            PFNMessageBox dpfnMessageBox=(PFNMessageBox) DecodePointer(pfnMessageBox);
            if(dpfnMessageBox)
            {
                return (dpfnMessageBox)(hWndParent, lpText, lpCaption, uType);
            }
            else
            {
                /* should never happen */
                return 0;
            }
        }
}
