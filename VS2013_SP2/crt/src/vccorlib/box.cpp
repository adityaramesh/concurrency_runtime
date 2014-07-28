//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//

#include "pch.h"
#pragma hdrstop

#include <windows.h>

#include <windows.foundation.h>

// Required to define private interface because
// DevDiv public SDK hasn't been updated to right version thus mismatches with IPropertyValue implemenation
// Make sure that this will be removed after SDK update
__interface IPropertyValuePriv :
public IInspectable
{
public:
	virtual HRESULT STDMETHODCALLTYPE get_Type(ABI::Windows::Foundation::PropertyType *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_IsNumericScalar(boolean *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt8(BYTE *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt16(INT16 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt16(UINT16 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt32(INT32 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt32(UINT32 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt64(INT64 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt64(UINT64 *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSingle(FLOAT *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDouble(DOUBLE *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetChar16(WCHAR *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBoolean(boolean *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetString(HSTRING *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetGuid(GUID *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDateTime(ABI::Windows::Foundation::DateTime *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTimeSpan(ABI::Windows::Foundation::TimeSpan *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPoint(ABI::Windows::Foundation::Point *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSize(ABI::Windows::Foundation::Size *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRect(ABI::Windows::Foundation::Rect *value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt8Array(UINT32 *__valueSize, BYTE **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt16Array(UINT32 *__valueSize, INT16 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt16Array(UINT32 *__valueSize, UINT16 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt32Array(UINT32 *__valueSize, INT32 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt32Array(UINT32 *__valueSize, UINT32 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInt64Array(UINT32 *__valueSize, INT64 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUInt64Array(UINT32 *__valueSize, UINT64 **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSingleArray(UINT32 *__valueSize, FLOAT **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDoubleArray(UINT32 *__valueSize, DOUBLE **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetChar16Array(UINT32 *__valueSize, WCHAR **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBooleanArray(UINT32 *__valueSize, boolean **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetStringArray(UINT32 *__valueSize, HSTRING **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInspectableArray(UINT32 *__valueSize, IInspectable ***value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetGuidArray(UINT32 *__valueSize, GUID **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDateTimeArray(UINT32 *__valueSize, ABI::Windows::Foundation::DateTime **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTimeSpanArray(UINT32 *__valueSize, ABI::Windows::Foundation::TimeSpan **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPointArray(UINT32 *__valueSize, ABI::Windows::Foundation::Point **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSizeArray(UINT32 *__valueSize, ABI::Windows::Foundation::Size **value) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRectArray(UINT32 *__valueSize, ABI::Windows::Foundation::Rect **value) = 0;
};

typedef IPropertyValuePriv IPropertyValue;

// Make sure that interface size hasn't been changed
static_assert(sizeof(::Windows::Foundation::IPropertyValue^) == sizeof(IPropertyValue) , "Winmd defined IPropertyValue has different size than IPropertyValue");

namespace Platform { namespace Details {

	// To extract valid IInspectable interface pointer we need to VTABLE pointer one position down
	// and cast it to IInspectable
	inline IInspectable* GetInspectable(void *pPropertyValuePtr)
	{
		auto ptr = reinterpret_cast<ULONG_PTR*>(pPropertyValuePtr);
		ptr--;
		auto pUnk = reinterpret_cast<IInspectable*>(ptr);
		_ASSERTE(pUnk != nullptr);
		return pUnk;
	}

	// VTABLE wrapper for custom interfaces for IBox
	class CustomPropertyValue :
		public IPropertyValue
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
		{
			return GetInspectable(this)->QueryInterface(riid, ppvObject);
		}

		virtual ULONG STDMETHODCALLTYPE AddRef(void)
		{ 
			return GetInspectable(this)->AddRef();
		}

		virtual ULONG STDMETHODCALLTYPE Release(void)
		{ 
			return GetInspectable(this)->Release();
		}

		virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG *iidCount, IID **iids)
		{ 
			return GetInspectable(this)->GetIids(iidCount, iids);
		}

		virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING *className)
		{ 
			return GetInspectable(this)->GetRuntimeClassName(className);
		}

		virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel *trustLevel)
		{ 
			return GetInspectable(this)->GetTrustLevel(trustLevel);
		}

		virtual HRESULT STDMETHODCALLTYPE get_Type(::ABI::Windows::Foundation::PropertyType *value)
		{
			*value = ::ABI::Windows::Foundation::PropertyType::PropertyType_OtherType;
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE get_IsNumericScalar(boolean *value)
		{
			*value = false;
			return S_OK; 
		}

		virtual HRESULT STDMETHODCALLTYPE GetUInt8(BYTE *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt16(INT16 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt16(UINT16 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt32(INT32 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt32(UINT32 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt64(INT64 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt64(UINT64 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSingle(FLOAT *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDouble(DOUBLE *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetChar16(WCHAR *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetBoolean(boolean *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetString(HSTRING *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetGuid(GUID *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDateTime(::ABI::Windows::Foundation::DateTime *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetTimeSpan(::ABI::Windows::Foundation::TimeSpan *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetPoint(::ABI::Windows::Foundation::Point *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSize(::ABI::Windows::Foundation::Size *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetRect(::ABI::Windows::Foundation::Rect *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt8Array(UINT32 *__valueSize, BYTE **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt16Array(UINT32 *__valueSize, INT16 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt16Array(UINT32 *__valueSize, UINT16 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt32Array(UINT32 *__valueSize, INT32 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt32Array(UINT32 *__valueSize, UINT32 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt64Array(UINT32 *__valueSize, INT64 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt64Array(UINT32 *__valueSize, UINT64 **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSingleArray(UINT32 *__valueSize, FLOAT **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDoubleArray(UINT32 *__valueSize, DOUBLE **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetChar16Array(UINT32 *__valueSize, WCHAR **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetBooleanArray(UINT32 *__valueSize, boolean **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetStringArray(UINT32 *__valueSize, HSTRING **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInspectableArray(UINT32 *__valueSize, IInspectable ***value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetGuidArray(UINT32 *__valueSize, GUID **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDateTimeArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::DateTime **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetTimeSpanArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::TimeSpan **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetPointArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Point **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSizeArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Size **value) { *value = nullptr; return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetRectArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Rect **value) { *value = nullptr; return E_NOTIMPL; }
	};

	//Make sure that implementation of boxes has correct VTABLE layout
	void ValidateVTableLayout(void* pBoxPtr)
	{
		IUnknown* pUnk = reinterpret_cast<IUnknown*>(pBoxPtr);
		Microsoft::WRL::ComPtr<IUnknown> pValueType;

		_ASSERTE(pUnk != nullptr);
		HRESULT hr = pUnk->QueryInterface(__uuidof(::Platform::IValueType^),  reinterpret_cast<void**>(pValueType.GetAddressOf()));
		if (FAILED(hr))
		{
			::RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
		}
		pUnk++;

		// After incrementing pUnk ptr we should get the pValueType pointer value otherwise
		// it means that vtables are not next to each other
		if (pUnk != pValueType.Get())
		{
			::RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
		}
	}

	VCCORLIB_API void* __stdcall GetIBoxVtable(void* pBoxPtr)
	{
		(pBoxPtr);
#ifdef DBG
		ValidateVTableLayout(pBoxPtr);    
#endif
		return (void*)__vtable_introduced_by(CustomPropertyValue, IPropertyValue);
	}

	template<typename T> class PropertyValueTraits;

	template<>
	class PropertyValueTraits<unsigned char>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<unsigned char>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateUInt8Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<short>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<short>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateInt16Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<unsigned short>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<unsigned short>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateUInt16Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<int>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<int>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateInt32Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<unsigned int>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<unsigned int>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateUInt32Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<__int64>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<__int64>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateInt64Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<unsigned __int64>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<unsigned __int64>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateUInt64Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<float>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<float>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateSingleArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<double>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<double>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateDoubleArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<__wchar_t>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<__wchar_t>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateChar16Array(arr);
		}
	};

	template<>
	class PropertyValueTraits<bool>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<bool>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateBooleanArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<::Platform::String^>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<::Platform::String^>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateStringArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<::Platform::Object^>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<::Platform::Object^>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateInspectableArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<::Platform::Guid>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<::Platform::Guid>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateGuidArray(arr);
		}
	};


	template<>
	class PropertyValueTraits<Windows::Foundation::DateTime>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<Windows::Foundation::DateTime>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateDateTimeArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<Windows::Foundation::TimeSpan>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<Windows::Foundation::TimeSpan>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateTimeSpanArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<Windows::Foundation::Point>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<Windows::Foundation::Point>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreatePointArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<Windows::Foundation::Size>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<Windows::Foundation::Size>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateSizeArray(arr);
		}
	};

	template<>
	class PropertyValueTraits<Windows::Foundation::Rect>
	{
	public:
		static ::Platform::Object^ CreatePropertyValue(::Platform::Array<Windows::Foundation::Rect>^ arr)
		{
			return ::Windows::Foundation::PropertyValue::CreateRectArray(arr);
		}
	};

	// Creating system IPropertyValue^ interface
	// We mix High level with low level because there is no public definitions of IReferenceArray in the *.h file
	template<typename T>
	inline HRESULT CreateSystemPropertyValue(IUnknown* pUnk, IPropertyValue** value)
	{    
		Platform::IBoxArray<T>^ pReference;
		HRESULT hr = pUnk->QueryInterface(__uuidof(Platform::IBoxArray<T>^), reinterpret_cast<void**>(&pReference));
		if (FAILED(hr))
		{
			return hr;
		}

		::Platform::Object ^propertyValue = nullptr;
		try {
			propertyValue = PropertyValueTraits<T>::CreatePropertyValue(pReference->Value);
		}
		catch(::Platform::Exception^ e)
		{
			return e->HResult;
		}

		return reinterpret_cast<IUnknown*>(const_cast< ::Platform::Object^>(propertyValue))->QueryInterface(
			__uuidof(::Windows::Foundation::IPropertyValue^), reinterpret_cast<void**>(value));
	}

	// Create IPropertyValue object from current wrapper for Array's
	HRESULT CreatePropertyValue(void* pPropertyValuePtr, IPropertyValue** value)
	{
		auto pInspectable = GetInspectable(pPropertyValuePtr);        
		unsigned long count = 0;
		IID* iids = nullptr;

		HRESULT hr = pInspectable->GetIids(&count, &iids);
		if (FAILED(hr))
		{
			return hr;
		}

		//Make sure that we return E_NOINTERFACE when no known IReferenceArray implemented
		hr = E_NOINTERFACE; 

		for(unsigned long i = 0; i < count; i++)
		{
			IID& current = iids[i];
			if (__uuidof(::Platform::Details::IWeakReferenceSource^) == current)
			{
				continue;
			}
			//QI for IReferenceArray interfaces
			else if (__uuidof(Platform::IBoxArray<unsigned char>^) == current)
			{
				hr = CreateSystemPropertyValue<unsigned char>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<short>^) == current)
			{
				hr = CreateSystemPropertyValue<short>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<unsigned short>^) == current)
			{
				hr = CreateSystemPropertyValue<unsigned short>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<int>^) == current)
			{
				hr = CreateSystemPropertyValue<int>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<unsigned int>^) == current)
			{
				hr = CreateSystemPropertyValue<unsigned int>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<__int64>^) == current)
			{    
				hr = CreateSystemPropertyValue<__int64>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<unsigned __int64>^) == current)
			{    
				hr = CreateSystemPropertyValue<unsigned __int64>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<float>^) == current)
			{    
				hr = CreateSystemPropertyValue<float>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<double>^) == current)
			{
				hr = CreateSystemPropertyValue<double>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<__wchar_t>^) == current)
			{
				hr = CreateSystemPropertyValue<__wchar_t>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<bool>^) == current)
			{
				hr = CreateSystemPropertyValue<bool>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<::Platform::String^>^) == current)
			{
				hr = CreateSystemPropertyValue<::Platform::String^>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<::Platform::Object^>^) == current)
			{
				hr = CreateSystemPropertyValue<::Platform::Object^>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<::Platform::Guid>^) == current)
			{
				hr = CreateSystemPropertyValue<::Platform::Guid>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<Windows::Foundation::DateTime>^) == current)
			{
				hr = CreateSystemPropertyValue<Windows::Foundation::DateTime>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<Windows::Foundation::TimeSpan>^) == current)
			{    
				hr = CreateSystemPropertyValue<Windows::Foundation::TimeSpan>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<Windows::Foundation::Point>^) == current)
			{
				hr = CreateSystemPropertyValue<Windows::Foundation::Point>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<Windows::Foundation::Size>^) == current)
			{
				hr = CreateSystemPropertyValue<Windows::Foundation::Size>(pInspectable, value);
				break;
			}
			else if (__uuidof(Platform::IBoxArray<Windows::Foundation::Rect>^) == current)
			{
				hr = CreateSystemPropertyValue<Windows::Foundation::Rect>(pInspectable, value);
				break;
			}
		}

		CoTaskMemFree(iids);
		return hr;
	}

	// Create VTABLE wrapper for Boxed array's
	class CustomPropertyValueArray :
		public IPropertyValue
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
		{
			return GetInspectable(this)->QueryInterface(riid, ppvObject);
		}

		virtual ULONG STDMETHODCALLTYPE AddRef(void)
		{ 
			return GetInspectable(this)->AddRef();
		}

		virtual ULONG STDMETHODCALLTYPE Release(void)
		{ 
			return GetInspectable(this)->Release();
		}

		virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG *iidCount, IID **iids)
		{ 
			return GetInspectable(this)->GetIids(iidCount, iids);
		}

		virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING *className)
		{
			return GetInspectable(this)->GetRuntimeClassName(className);
		}

		virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel *trustLevel)
		{ 
			return GetInspectable(this)->GetTrustLevel(trustLevel);
		}

		virtual HRESULT STDMETHODCALLTYPE get_Type(::ABI::Windows::Foundation::PropertyType *value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				*value = ::ABI::Windows::Foundation::PropertyType::PropertyType_OtherTypeArray;
				return S_OK;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->get_Type(value);
		}

		virtual HRESULT STDMETHODCALLTYPE get_IsNumericScalar(boolean *value)
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				*value = false;
				return S_OK;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->get_IsNumericScalar(value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetUInt8(BYTE *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt16(INT16 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt16(UINT16 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt32(INT32 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt32(UINT32 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetInt64(INT64 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetUInt64(UINT64 *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSingle(FLOAT *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDouble(DOUBLE *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetChar16(WCHAR *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetBoolean(boolean *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetString(HSTRING *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetGuid(GUID *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetDateTime(::ABI::Windows::Foundation::DateTime *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetTimeSpan(::ABI::Windows::Foundation::TimeSpan *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetPoint(::ABI::Windows::Foundation::Point *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetSize(::ABI::Windows::Foundation::Size *value) { return E_NOTIMPL; }
		virtual HRESULT STDMETHODCALLTYPE GetRect(::ABI::Windows::Foundation::Rect *value) { return E_NOTIMPL; }

		virtual HRESULT STDMETHODCALLTYPE GetUInt8Array(UINT32 *__valueSize, BYTE **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetUInt8Array(__valueSize, value);
		}
		virtual HRESULT STDMETHODCALLTYPE GetInt16Array(UINT32 *__valueSize, INT16 **value) 
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetInt16Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetUInt16Array(UINT32 *__valueSize, UINT16 **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetUInt16Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetInt32Array(UINT32 *__valueSize, INT32 **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetInt32Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetUInt32Array(UINT32 *__valueSize, UINT32 **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetUInt32Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetInt64Array(UINT32 *__valueSize, INT64 **value)
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetInt64Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetUInt64Array(UINT32 *__valueSize, UINT64 **value) 
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetUInt64Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetSingleArray(UINT32 *__valueSize, FLOAT **value) 
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetSingleArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetDoubleArray(UINT32 *__valueSize, DOUBLE **value)
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetDoubleArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetChar16Array(UINT32 *__valueSize, WCHAR **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetChar16Array(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetBooleanArray(UINT32 *__valueSize, boolean **value)
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetBooleanArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetStringArray(UINT32 *__valueSize, HSTRING **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetStringArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetInspectableArray(UINT32 *__valueSize, IInspectable ***value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetInspectableArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetGuidArray(UINT32 *__valueSize, GUID **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetGuidArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetDateTimeArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::DateTime **value) 
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetDateTimeArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetTimeSpanArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::TimeSpan **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetTimeSpanArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetPointArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Point **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetPointArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetSizeArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Size **value)
		{
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetSizeArray(__valueSize, value);
		}

		virtual HRESULT STDMETHODCALLTYPE GetRectArray(UINT32 *__valueSize, ::ABI::Windows::Foundation::Rect **value)
		{ 
			Microsoft::WRL::ComPtr<IPropertyValue> propertyValue;
			HRESULT hr = CreatePropertyValue(this, propertyValue.GetAddressOf());
			if (hr == E_NOINTERFACE)
			{
				return E_NOTIMPL;
			}
			else if (FAILED(hr))
			{
				return hr;
			}

			return propertyValue->GetRectArray(__valueSize, value);
		}
	};

	VCCORLIB_API void* __stdcall GetIBoxArrayVtable(void* pBoxPtr)
	{
		(pBoxPtr);
#ifdef DBG
		ValidateVTableLayout(pBoxPtr);
#endif
		return (void*)__vtable_introduced_by(CustomPropertyValueArray, IPropertyValue);
	}

	VCCORLIB_API ::Platform::Object^ __stdcall CreateValue(Platform::TypeCode typeCode, const void* constValue)
	{
		::Platform::Object^ propertyValue = nullptr;
		void* value = const_cast<void*>(constValue);

#pragma warning(push)
#pragma warning(disable: 4061)
		switch(typeCode)
		{
		case ::Platform::TypeCode::UInt8:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateUInt8(*reinterpret_cast<unsigned char*>(value));
			break;
		case ::Platform::TypeCode::Int16:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateInt16(*reinterpret_cast<short*>(value));
			break;
		case ::Platform::TypeCode::UInt16:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateUInt16(*reinterpret_cast<unsigned short*>(value));
			break;
		case ::Platform::TypeCode::Int32:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateInt32(*reinterpret_cast<int*>(value));
			break;
		case ::Platform::TypeCode::UInt32:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateUInt32(*reinterpret_cast<unsigned int*>(value));
			break;
		case ::Platform::TypeCode::Int64:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateInt64(*reinterpret_cast<__int64*>(value));
			break;
		case ::Platform::TypeCode::UInt64:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateUInt64(*reinterpret_cast<unsigned __int64*>(value));
			break;
		case ::Platform::TypeCode::Single:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateSingle(*reinterpret_cast<float*>(value));
			break;
		case ::Platform::TypeCode::Double:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateDouble(*reinterpret_cast<double*>(value));
			break;
		case ::Platform::TypeCode::Char16:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateChar16(*reinterpret_cast<wchar_t*>(value));
			break;
		case ::Platform::TypeCode::Boolean:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateBoolean(*reinterpret_cast<bool*>(value));
			break;
		case ::Platform::TypeCode::DateTime:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateDateTime(*reinterpret_cast<::Windows::Foundation::DateTime*>(value));
			break;
		case ::Platform::TypeCode::TimeSpan:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateTimeSpan(*reinterpret_cast<::Windows::Foundation::TimeSpan*>(value));
			break;
		case ::Platform::TypeCode::Point:
			propertyValue = ::Windows::Foundation::PropertyValue::CreatePoint(*reinterpret_cast<::Windows::Foundation::Point*>(value));
			break;
		case ::Platform::TypeCode::Size:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateSize(*reinterpret_cast<::Windows::Foundation::Size*>(value));
			break;
		case ::Platform::TypeCode::Rect:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateRect(*reinterpret_cast<::Windows::Foundation::Rect*>(value));
			break;
		case ::Platform::TypeCode::Guid:
			propertyValue = ::Windows::Foundation::PropertyValue::CreateGuid(*reinterpret_cast<::Platform::Guid*>(value));
			break;
		}
#pragma warning(pop)

		// Make sure that IPropertyValue is the default interface
		_ASSERTE(propertyValue == nullptr || safe_cast<::Windows::Foundation::IPropertyValue^>(propertyValue) == propertyValue);
		return propertyValue;
	}

} } // namespace Platform::Details
