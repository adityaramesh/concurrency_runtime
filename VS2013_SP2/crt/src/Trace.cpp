// ==++==
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
// 
// ==--==
// =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Trace.cpp
//
// Implementation of ConcRT tracing API.
//
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#include "concrtinternal.h"

namespace Concurrency
{
namespace details
{
    TRACEHANDLE g_ConcRTPRoviderHandle;
    TRACEHANDLE g_ConcRTSessionHandle;

    _CONCRT_TRACE_INFO g_TraceInfo = {0};

    Etw* g_pEtw = NULL;

    Etw::Etw() throw()
    {
#if defined(_CRT_APP) || defined(_KERNELX)
        // No ETW support
#else

#ifdef _CORESYS
#define TRACE_DLL L"api-ms-win-eventing-classicprovider-l1-1-0.dll"
#else
#define TRACE_DLL L"advapi32.dll"
#endif
        HMODULE hTraceapi = LoadLibraryExW(TRACE_DLL, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#ifndef _CORESYS
        if (!hTraceapi && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom, but TRACE_DLL is a KnownDLL so it is safe to load
            // it without supplying the full path. On Windows 8 and higher, LOAD_LIBRARY_SEARCH_SYSTEM32 should be used as a best practice.
            hTraceapi = LoadLibraryW(TRACE_DLL);
        }
#endif
        if (hTraceapi != NULL)
        {
            m_pfnRegisterTraceGuidsW = (FnRegisterTraceGuidsW*) Security::EncodePointer(GetProcAddress(hTraceapi, "RegisterTraceGuidsW"));
            m_pfnUnregisterTraceGuids = (FnUnregisterTraceGuids*) Security::EncodePointer(GetProcAddress(hTraceapi, "UnregisterTraceGuids"));
            m_pfnTraceEvent = (FnTraceEvent*) Security::EncodePointer(GetProcAddress(hTraceapi, "TraceEvent"));
            m_pfnGetTraceLoggerHandle = (FnGetTraceLoggerHandle*) Security::EncodePointer(GetProcAddress(hTraceapi, "GetTraceLoggerHandle"));
            m_pfnGetTraceEnableLevel = (FnGetTraceEnableLevel*) Security::EncodePointer(GetProcAddress(hTraceapi, "GetTraceEnableLevel"));
            m_pfnGetTraceEnableFlags = (FnGetTraceEnableFlags*) Security::EncodePointer(GetProcAddress(hTraceapi, "GetTraceEnableFlags"));
        }

#endif // defined(_CRT_APP) || defined(_KERNELX)
    }

    ULONG Etw::RegisterGuids(WMIDPREQUEST controlCallBack, LPCGUID providerGuid, ULONG guidCount, PTRACE_GUID_REGISTRATION eventGuidRegistration, PTRACEHANDLE providerHandle)
    {
        if (m_pfnRegisterTraceGuidsW != EncodePointer(NULL))
        {
            FnRegisterTraceGuidsW* pfnRegisterTraceGuidsW = (FnRegisterTraceGuidsW*) Security::DecodePointer(m_pfnRegisterTraceGuidsW);
            return pfnRegisterTraceGuidsW(controlCallBack, NULL, providerGuid, guidCount, eventGuidRegistration, NULL, NULL, providerHandle);
        }

        return ERROR_PROC_NOT_FOUND;
    }

    ULONG Etw::UnregisterGuids(TRACEHANDLE handle)
    {
        if (m_pfnUnregisterTraceGuids != EncodePointer(NULL))
        {
            FnUnregisterTraceGuids* pfnUnregisterTraceGuids = (FnUnregisterTraceGuids*) Security::DecodePointer(m_pfnUnregisterTraceGuids);
            return pfnUnregisterTraceGuids(handle);
        }

        return ERROR_PROC_NOT_FOUND;
    }

    ULONG Etw::Trace(TRACEHANDLE handle, PEVENT_TRACE_HEADER eventHeader)
    {
        if (m_pfnTraceEvent != EncodePointer(NULL))
        {
            FnTraceEvent* pfnTraceEvent = (FnTraceEvent*) Security::DecodePointer(m_pfnTraceEvent);
            return pfnTraceEvent(handle, eventHeader);
        }

        return ERROR_PROC_NOT_FOUND;
    }

    TRACEHANDLE Etw::GetLoggerHandle(PVOID buffer)
    {
        if (m_pfnGetTraceLoggerHandle != EncodePointer(NULL))
        {
            FnGetTraceLoggerHandle* pfnGetTraceLoggerHandle = (FnGetTraceLoggerHandle*) Security::DecodePointer(m_pfnGetTraceLoggerHandle);
            return pfnGetTraceLoggerHandle(buffer);
        }

        SetLastError(ERROR_PROC_NOT_FOUND);
        return (TRACEHANDLE)INVALID_HANDLE_VALUE;
    }

    UCHAR Etw::GetEnableLevel(TRACEHANDLE handle)
    {
        if (m_pfnGetTraceEnableLevel != EncodePointer(NULL))
        {
            FnGetTraceEnableLevel* pfnGetTraceEnableLevel = (FnGetTraceEnableLevel*) Security::DecodePointer(m_pfnGetTraceEnableLevel);
            return pfnGetTraceEnableLevel(handle);
        }

        SetLastError(ERROR_PROC_NOT_FOUND);
        return 0;
    }

    ULONG Etw::GetEnableFlags(TRACEHANDLE handle)
    {
        if (m_pfnGetTraceEnableFlags != EncodePointer(NULL))
        {
            FnGetTraceEnableFlags* pfnGetTraceEnableFlags = (FnGetTraceEnableFlags*) Security::DecodePointer(m_pfnGetTraceEnableFlags);
            return pfnGetTraceEnableFlags(handle);
        }

        SetLastError(ERROR_PROC_NOT_FOUND);
        return 0;
    }


    /// <summary>WMI control call back</summary>
    ULONG WINAPI ControlCallback(WMIDPREQUESTCODE requestCode, void* requestContext, ULONG* reserved, void* buffer)
    {
        DWORD rc;

        switch (requestCode)
        {
        case WMI_ENABLE_EVENTS:     
            // Enable the provider
            {
                g_ConcRTSessionHandle = g_pEtw->GetLoggerHandle(buffer);
                if ((HANDLE)g_ConcRTSessionHandle == INVALID_HANDLE_VALUE)
                    return GetLastError();

                SetLastError(ERROR_SUCCESS);
                ULONG level = g_pEtw->GetEnableLevel(g_ConcRTSessionHandle);
                if (level == 0)
                {
                    rc = GetLastError();
                    if (rc == ERROR_SUCCESS)
                    {
                        // Enable level of 0 means TRACE_LEVEL_INFORMATION
                        level = TRACE_LEVEL_INFORMATION;
                    }
                    else
                    {
                        return rc;
                    }
                }

                ULONG flags = g_pEtw->GetEnableFlags(g_ConcRTSessionHandle);
                if (flags == 0)
                {
                    rc = GetLastError();
                    if (rc == ERROR_SUCCESS)
                    {
                        flags = AllEventsFlag;
                    }
                    else
                    {
                        return rc;
                    }
                }

                // Tracing is now enabled.
                g_TraceInfo._EnableTrace((unsigned char)level, (unsigned long)flags);
            }

            break;

        case WMI_DISABLE_EVENTS:    // Disable the provider
            g_TraceInfo._DisableTrace();
            g_ConcRTSessionHandle = 0;
            break;

        case WMI_EXECUTE_METHOD:
        case WMI_REGINFO:
        case WMI_DISABLE_COLLECTION:
        case WMI_ENABLE_COLLECTION:
        case WMI_SET_SINGLE_ITEM:
        case WMI_SET_SINGLE_INSTANCE:
        case WMI_GET_SINGLE_INSTANCE:
        case WMI_GET_ALL_DATA:
        default:
            return ERROR_INVALID_PARAMETER;
        }

        return ERROR_SUCCESS;
    }

    void PPL_Trace_Event(const GUID& guid, ConcRT_EventType eventType, UCHAR level)
    {
        if (g_pEtw != NULL)
        {
            CONCRT_TRACE_EVENT_HEADER_COMMON concrtHeader = {0};

            concrtHeader.header.Size = sizeof concrtHeader;
            concrtHeader.header.Flags = WNODE_FLAG_TRACED_GUID;
            concrtHeader.header.Guid = guid;
            concrtHeader.header.Class.Type = (UCHAR) eventType;
            concrtHeader.header.Class.Level = level;

            g_pEtw->Trace(g_ConcRTSessionHandle, &concrtHeader.header);
        }
    }

    void _RegisterConcRTEventTracing()
    {
#if defined(_CRT_APP) || defined(_KERNELX)
        g_pEtw = NULL;
#else
        // Initialize ETW dynamically, and only once, to avoid a static dependency on Advapi32.dll.
        _StaticLock::_Scoped_lock lockHolder(Etw::s_lock);

        if (g_pEtw == NULL)
        {
            g_pEtw = _concrt_new Etw();

            static TRACE_GUID_REGISTRATION eventGuidRegistration[] = {
                { &Concurrency::ConcRTEventGuid, NULL },
                { &Concurrency::SchedulerEventGuid, NULL },
                { &Concurrency::ScheduleGroupEventGuid, NULL },
                { &Concurrency::ContextEventGuid, NULL },
                { &Concurrency::ChoreEventGuid, NULL },
                { &Concurrency::LockEventGuid, NULL },
                { &Concurrency::ResourceManagerEventGuid, NULL }
            };

            ULONG eventGuidCount = sizeof eventGuidRegistration / sizeof eventGuidRegistration[0];

            ULONG rc = g_pEtw->RegisterGuids(Concurrency::details::ControlCallback, &ConcRT_ProviderGuid, eventGuidCount, eventGuidRegistration, &g_ConcRTPRoviderHandle);
        }
#endif // defined(_CRT_APP) || defined(_KERNELX)
    }

    void _UnregisterConcRTEventTracing()
    {
        if (g_pEtw != NULL)
        {
            ENSURE_NOT_APP_OR_KERNELX();
            g_TraceInfo._DisableTrace();
            ULONG rc = g_pEtw->UnregisterGuids(g_ConcRTPRoviderHandle);
            delete g_pEtw;
            g_pEtw = NULL;
        }
    }
} // namespace details

/// <summary>
///     Enable tracing
/// </summary>
/// <returns>
///     If tracing was correctly initiated, S_OK is returned, otherwise E_NOT_STARTED is returned
/// </returns>
_CRTIMP HRESULT EnableTracing()
{
    // Deprecated
    return S_OK;
}


/// <summary>
///     Disables tracing
/// </summary>
/// <returns>
///     If tracing was correctly disabled, S_OK is returned.  If tracing was not previously initiated,
///     E_NOT_STARTED is returned
/// </returns>
_CRTIMP HRESULT DisableTracing()
{
    // Deprecated
    return S_OK;
}

_CRTIMP const _CONCRT_TRACE_INFO* _GetConcRTTraceInfo()
{
    if (g_pEtw == NULL)
    {
        ::Concurrency::details::_RegisterConcRTEventTracing();
    }

    return &g_TraceInfo;
}

// Trace an event signaling the begin of a PPL function
_CRTIMP void _Trace_ppl_function(const GUID& guid, UCHAR level, ConcRT_EventType type)
{
    const _CONCRT_TRACE_INFO * traceInfo = _GetConcRTTraceInfo();

    if (traceInfo->_IsEnabled(level, PPLEventFlag))
        Concurrency::details::PPL_Trace_Event(guid, type, level);
}

// Trace an event from the Agents library
_CRTIMP void _Trace_agents(::Concurrency::Agents_EventType eventType, __int64 agentId, ...)
{
    va_list args;
    va_start(args, agentId);

    UCHAR level = TRACE_LEVEL_INFORMATION;
    const _CONCRT_TRACE_INFO * traceInfo = _GetConcRTTraceInfo();

    if (traceInfo->_IsEnabled(level, AgentEventFlag))
    {
        AGENTS_TRACE_EVENT_DATA agentsData = {0};

        // Header
        agentsData.header.Size = sizeof(agentsData);
        agentsData.header.Flags = WNODE_FLAG_TRACED_GUID;
        agentsData.header.Guid = AgentEventGuid;
        agentsData.header.Class.Type = (UCHAR) eventType;
        agentsData.header.Class.Level = level;

        // Payload
        agentsData.payload.AgentId1 = agentId;
        
        switch (eventType)
        {
        case AGENTS_EVENT_CREATE:
            // LWT id
            agentsData.payload.AgentId2 = va_arg(args, __int64);
            break;

        case AGENTS_EVENT_DESTROY:
            // No other payload
            break;

        case AGENTS_EVENT_LINK:
        case AGENTS_EVENT_UNLINK:
            // Target's id
            agentsData.payload.AgentId2 = va_arg(args, __int64);
            break;

        case AGENTS_EVENT_START:
            // No other payload
            break;

        case AGENTS_EVENT_END:
            // Number of messages processed
            agentsData.payload.Count = va_arg(args, long);
            break;

        case AGENTS_EVENT_SCHEDULE:
            // No other payload
            break;

        case AGENTS_EVENT_NAME:
            {
                wchar_t * name = va_arg(args, wchar_t*);

                if (name != NULL)
                {
                    wcsncpy_s(agentsData.payload.Name, _countof(agentsData.payload.Name), name, _TRUNCATE);
                }
            }
            break;

        default:
            break;
        };

        va_end(args);
        g_pEtw->Trace(g_ConcRTSessionHandle, &agentsData.header);
    }
}

} // namespace Concurrency
