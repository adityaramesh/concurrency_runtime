//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//

#include "pch.h"
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include "activation.h"
#pragma hdrstop
#include <Strsafe.h>
#include <memory>

using namespace Microsoft::WRL;
using namespace std;

struct conditional_deletor
{
    conditional_deletor(bool bDelete = true) : _bDelete(bDelete)
    {
    }
    conditional_deletor(const conditional_deletor& other) : _bDelete(other._bDelete)
    {
    }
    conditional_deletor(conditional_deletor&& other) : _bDelete(other._bDelete)
    {
        other._bDelete = true;
    }
    conditional_deletor& operator=(conditional_deletor other)
    {
        _bDelete = other._bDelete;
    }
    void operator()(const wchar_t* p)
    {
        if (_bDelete)
        {
            delete p;
        }
        return;
    }
private:
    bool _bDelete;
};

template <typename deletor>
struct hash_unique_wchar
{
    size_t operator()(const unique_ptr<const wchar_t, deletor>& ele)
    {
        return hash_value(reinterpret_cast<const wchar_t*>(ele.get()));
    }
};

template <typename deletor>
struct equal_unique_wchar
{
    bool operator()(const unique_ptr<const wchar_t, deletor>& first, const unique_ptr<const wchar_t, deletor>& second)
    {
        return wcscmp(first.get(), second.get()) == 0;
    }
};

CPPCLI_FUNC HRESULT __stdcall __getActivationFactoryByHSTRING(HSTRING str, ::Platform::Guid& riid, PVOID * ppActivationFactory)
{
    HRESULT hr = S_OK;
    IActivationFactory* pActivationFactory;
    hr = Windows::Foundation::GetActivationFactory(str, &pActivationFactory);
    if (SUCCEEDED(hr))
    {
        hr = pActivationFactory->QueryInterface(reinterpret_cast<REFIID>(riid), ppActivationFactory);
        pActivationFactory->Release();
    }

    return hr;
}

class PerApartmentFactoryCache : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IApartmentShutdown, FtmBase>
{
    unordered_map<unique_ptr<const wchar_t, conditional_deletor>, ComPtr<IUnknown>, hash_unique_wchar<conditional_deletor>, equal_unique_wchar<conditional_deletor>> _factoryCache;
    Wrappers::CriticalSection _criticalSection;
    UINT64 _appartmentId;
    bool _apartmentIsSTA;
public:
    HRESULT RuntimeClassInitialize(UINT64 appartmentId)
    {
        _appartmentId = appartmentId;
        APTTYPE AptType;
        APTTYPEQUALIFIER AptQualifier;
        HRESULT hr = CoGetApartmentType(&AptType, &AptQualifier);
        if (SUCCEEDED(hr) && (AptType == APTTYPE::APTTYPE_MTA || AptType == APTTYPE::APTTYPE_NA))
        {
            _apartmentIsSTA = false;
        }
        else
        {
            _apartmentIsSTA = true;
        }
        return S_OK;
    }
    
    HRESULT AddFactory(const wchar_t* acid, IUnknown *factory)
    {
        try
        {
            auto len = wcslen(acid) + 1;
            wchar_t* acidCopy = new wchar_t[len];
            wcscpy_s(acidCopy, len, acid);
            unique_ptr<const wchar_t, conditional_deletor> activatableID(acidCopy);

            auto lock  = _criticalSection.Lock();
            auto ret = _factoryCache.insert(make_pair(std::move(activatableID), factory));
            if (ret.second == false)
            {
                return S_FALSE;
            }
            return S_OK;
        }
        catch (const bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        catch (const exception&)
        {
            return E_FAIL;
        }
    }

    HRESULT GetFactory(const wchar_t* acid, Platform::Guid& iid, void** pFactory)
    {
        *pFactory = nullptr;
        try
        {
            // Don't delete buffer
            unique_ptr<const wchar_t, conditional_deletor> activatableID(acid, false);
            decltype(_factoryCache.find(std::move(activatableID))) it;

            if (!_apartmentIsSTA)
            {
                auto lock  = _criticalSection.Lock();
                it = _factoryCache.find(std::move(activatableID));
                if (it == _factoryCache.end())
                {
                    return E_FAIL;
                }
            }
            else
            {
                it = _factoryCache.find(std::move(activatableID));
                if (it == _factoryCache.end())
                {
                    return E_FAIL;
                }
            }
            return it->second.CopyTo(iid, pFactory);
        }
        catch (const bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
        catch (const exception&)
        {
            return E_FAIL;
        }
    }
    
    void __stdcall OnUninitialize(UINT64 apartmentIdentifier);
};

class FactoryCache
{
    vector<pair<UINT64, pair<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, ComPtr<PerApartmentFactoryCache>>>> perApartmentCache;
    Wrappers::CriticalSection _criticalSection;
    static volatile long _cacheEnabled;
    static volatile long _cacheDestroyed;
public:
    static void Enable()
    {
        ::_InterlockedCompareExchange(&_cacheEnabled, 1, 0);
    }
    static void Disable()
    {
        ::_InterlockedCompareExchange(&_cacheEnabled, 0, 1);
    }
    static bool IsEnabled()
    {
        return (_cacheEnabled == 1);
    }    
    static bool IsDestroyed()
    {
        return (_cacheDestroyed != 0);
    }

    ~FactoryCache()
    {
        Disable();
        ::_InterlockedIncrement(&_cacheDestroyed);
        Flush();
    }
    void Flush()
    {
        auto lock  = _criticalSection.Lock();
        for(auto it = perApartmentCache.begin(); it != perApartmentCache.end(); it++)
        {
            ::RoUnregisterForApartmentShutdown(it->second.first);
        }
        perApartmentCache.clear();
    }
    HRESULT GetFactory(LPCWSTR acid, Platform::Guid& iid, void** pFactory)
    {    
        UINT64 apartmentID;
        HRESULT hr = ::RoGetApartmentIdentifier(&apartmentID);
        ComPtr<PerApartmentFactoryCache> apartmentCache;
        if (SUCCEEDED(hr) && IsEnabled())
        {
            auto lock  = _criticalSection.Lock();
            for(auto it = perApartmentCache.begin(); it != perApartmentCache.end(); it++)
            {
                if (it->first == apartmentID)
                {
                    apartmentCache = it->second.second;
                    lock.Unlock();
                    hr = apartmentCache->GetFactory(acid, iid, pFactory);
                    if (SUCCEEDED(hr))
                    {
                        return hr;
                    }
                    break;
                }
            }
            if (apartmentCache == nullptr)
            {            
                hr = MakeAndInitialize<PerApartmentFactoryCache>(&apartmentCache, apartmentID);
                if (SUCCEEDED(hr))
                {
                    UINT64 regAppartmentId;
                    APARTMENT_SHUTDOWN_REGISTRATION_COOKIE regCookie;
                    hr = ::RoRegisterForApartmentShutdown(apartmentCache.Get(), &regAppartmentId, &regCookie);
                    if (SUCCEEDED(hr))
                    {
                        __WRL_ASSERT__(regAppartmentId == apartmentID);
                        perApartmentCache.push_back(pair<UINT64, 
                            pair<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, ComPtr<PerApartmentFactoryCache>>>
                                (apartmentID, pair<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, ComPtr<PerApartmentFactoryCache>>(regCookie, apartmentCache.Get())));
                    }
                }
                
            }
        }

        // Create Factory
        HSTRING className;
        HSTRING_HEADER classNameHeader;
        hr = ::WindowsCreateStringReference(acid, static_cast<UINT32>(wcslen(acid)), &classNameHeader, &className); 
        if (FAILED(hr))
        {
            return hr;
        }

        ComPtr<IUnknown> factory;
        Platform::Guid riidUnknown(__uuidof(IUnknown));
        hr = __getActivationFactoryByHSTRING(className, riidUnknown, &factory);

        ::WindowsDeleteString(className);

        if (FAILED(hr))
        {
            return hr;
        }
            
        if (apartmentCache != nullptr)
        {
            apartmentCache->AddFactory(acid, factory.Get());
        }
    
        return factory.CopyTo(iid, pFactory);        
    }

    void RemoveApartmentCache(UINT64 apartmentIdentifier)
    {
        auto lock  = _criticalSection.Lock();
        
        for(auto it = perApartmentCache.begin(); it != perApartmentCache.end(); it++)
        {
            if (it->first == apartmentIdentifier)
            {
                perApartmentCache.erase(it);
                break;
            }
        }
    }
};

volatile long FactoryCache::_cacheEnabled = 0;
volatile long FactoryCache::_cacheDestroyed = 0;
FactoryCache g_FactoryCache;

void __stdcall PerApartmentFactoryCache::OnUninitialize(UINT64 apartmentIdentifier)
{
    if (apartmentIdentifier == _appartmentId)
    {
        g_FactoryCache.RemoveApartmentCache(apartmentIdentifier);
    }
}

CPPCLI_FUNC void EnableFactoryCache()
{
    FactoryCache::Enable();
}

void DisableFactoryCache()
{
    FactoryCache::Disable();
}

CPPCLI_FUNC void __stdcall FlushFactoryCache()
{
    if (!FactoryCache::IsDestroyed())
    {
        g_FactoryCache.Flush();
    }
}

CPPCLI_FUNC HRESULT __stdcall GetActivationFactoryByPCWSTR(void* str, ::Platform::Guid& riid, void** ppActivationFactory)
{
    wchar_t* acid =  static_cast<wchar_t*>(str);
    if (!FactoryCache::IsEnabled())
    {
        HSTRING className;
        HSTRING_HEADER classNameHeader;
        HRESULT hr = ::WindowsCreateStringReference(acid, static_cast<UINT32>(wcslen(acid)), &classNameHeader, &className); 
        if (SUCCEEDED(hr))
        {
            hr = __getActivationFactoryByHSTRING(className, riid, ppActivationFactory);
            ::WindowsDeleteString(className);
        }
        return hr;
    }
    return g_FactoryCache.GetFactory(acid, riid, ppActivationFactory);
}

CPPCLI_FUNC HRESULT __stdcall GetIidsFn(int nIids, unsigned long* iidCount, const __s_GUID* pIids, ::Platform::Guid** ppDuplicated)
{
    int nBytes = nIids * sizeof(::Platform::Guid);

    *ppDuplicated = (::Platform::Guid*)CoTaskMemAlloc(nBytes);
    if (*ppDuplicated)
    {
        memcpy(*ppDuplicated, pIids, nBytes);
        *iidCount = nIids;
        return S_OK;
    }

    *iidCount = 0;
    return E_OUTOFMEMORY;
}

#include "compiler.cpp"
#include "activation.cpp"


#pragma warning( disable: 4073 )  // initializers put in library initialization area
#pragma init_seg( lib )

#include "ehdata.h"
extern "C" void __cdecl _SetWinRTOutOfMemoryExceptionCallback(PGETWINRT_OOM_EXCEPTION pCallback);

namespace Platform { namespace Details {
	extern bool  __abi_firstAlloc;
	extern bool  __abi_is_global_oom_init;
	extern void* __abi_oom_controlblock;
	extern IUnknown* __abi_oom_singleton;
} }

void* __stdcall GetOutOfMemoryExceptionCallback()
{
	Platform::Details::__abi_oom_singleton->AddRef();
	return Platform::Details::__abi_oom_singleton;
}

class CInitExceptions
{
public:
	CInitExceptions()
	{
		// Ignore the effect on the ref new below on __abi_firstAlloc.
		bool wasFirstAlloc = Platform::Details::__abi_firstAlloc;

		Platform::Details::__abi_is_global_oom_init = true;

		// Would take down the process if it throws. This is fine.
		Platform::OutOfMemoryException^ oom = ref new Platform::OutOfMemoryException();

		Platform::Details::__abi_is_global_oom_init = false;

		// Recover firstalloc, so that user can set TrackingLevel in main.
		Platform::Details::__abi_firstAlloc = wasFirstAlloc;

		Platform::Details::__abi_oom_singleton = reinterpret_cast<IUnknown*>(oom);
		Platform::Details::__abi_oom_singleton->AddRef();

		_SetWinRTOutOfMemoryExceptionCallback(GetOutOfMemoryExceptionCallback);
	}

	~CInitExceptions()
	{
		_SetWinRTOutOfMemoryExceptionCallback(nullptr);
		Platform::Details::__abi_oom_singleton->Release();
	}
};

CInitExceptions initExceptions;
