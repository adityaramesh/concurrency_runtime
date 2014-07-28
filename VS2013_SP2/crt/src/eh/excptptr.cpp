/***
*excptptr.cpp - The exception_ptr implementation and everything associated with it.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*        The exception_ptr implementation and everything associated with it.
****/

#define _CRTIMP2_PURE

// This lives in R, so we can't drag in _Debug_message() which lives in P.
// We're using valid memory orders, so this has no effect.
#define _INVALID_MEMORY_ORDER

#include <internal.h>
#include <ehdata.h>
#include <trnsctrl.h>
#include <malloc.h>
#include <eh.h>
#include <mtdll.h>
#include <memory>
#include <exception>
#include <dbgint.h>
#include <stddef.h>
#include <new>
#include <stdexcept>
#include <windows.h>
#include <unknwn.h>

// Pre-V4 managed exception code
#define MANAGED_EXCEPTION_CODE  0XE0434F4D

// V4 and later managed exception code
#define MANAGED_EXCEPTION_CODE_V4  0XE0434352

#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)

#undef CT_COPYFUNC
#define CT_COPYFUNC(ct)         ((ct).copyFunction? CT_COPYFUNC_IB(ct, _GetThrowImageBase()):NULL)
#undef THROW_COUNT
#define THROW_COUNT(ti)         THROW_COUNT_IB(ti, _GetThrowImageBase())

#endif // _EH_RELATIVE_OFFSETS

////////////////////////////////////////////////////////////////////////////////
//
// Forward declaration of functions:
//

extern "C" _CRTIMP void * __cdecl __AdjustPointer( void *, const PMD&); // defined in frame.cpp

#define _pCurrentException      (*((EHExceptionRecord **)&(_getptd()->_curexception)))
#define _pCurrentExContext      (*((CONTEXT **)&(_getptd()->_curcontext)))
#define __ProcessingThrow       _getptd()->_ProcessingThrow

using namespace std;

class __ExceptionPtr
{
    public:
        static shared_ptr<__ExceptionPtr> _CurrentException();
        static shared_ptr<__ExceptionPtr> _CopyException(_In_ const void*,  _In_ const ThrowInfo*, bool normal = true);
        static shared_ptr<__ExceptionPtr> _BadAllocException();

        ~__ExceptionPtr();

        void _RethrowException() const;

    private:
        static shared_ptr<__ExceptionPtr> m_badAllocExceptionPtr;
        static shared_ptr<__ExceptionPtr> _InitBadAllocException();

        explicit __ExceptionPtr( _In_ const EHExceptionRecord *, bool normal = true);
        __ExceptionPtr( _In_ const __ExceptionPtr&); // not implemented
        __ExceptionPtr& operator=( _In_ const __ExceptionPtr&); // not implemented

        void _CallCopyCtor(_Out_writes_bytes_(size) void* dst, _In_reads_bytes_(size) void* src, size_t size, _In_ const CatchableType * const pType) const;
#if _EH_RELATIVE_OFFSETS
        ptrdiff_t _GetThrowImageBase() const
        { return (ptrdiff_t) m_EHRecord.params.pThrowImageBase; }
#endif
        union
        {
            _EXCEPTION_RECORD m_Record;
            EHExceptionRecord m_EHRecord;
        };
        bool m_normal;
};

static_assert(sizeof(std::exception_ptr) == sizeof(std::shared_ptr<__ExceptionPtr>),
    "std::exception_ptr and std::shared_ptr<__ExceptionPtr> must have the same size.");

shared_ptr<__ExceptionPtr> __ExceptionPtr::m_badAllocExceptionPtr = __ExceptionPtr::_InitBadAllocException();

shared_ptr<__ExceptionPtr> __ExceptionPtr::_InitBadAllocException()
{
    std::bad_alloc _Except;
    return __ExceptionPtr::_CopyException(&_Except, static_cast<const ThrowInfo*>(__GetExceptionInfo(_Except)), false);
}

shared_ptr<__ExceptionPtr> __ExceptionPtr::_BadAllocException()
{
    return __ExceptionPtr::m_badAllocExceptionPtr;
}

////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::_CallCopyCtor();
//
// Helper function for calling the copy-ctor on the C++ exception object.  It reads
// the passed info throw info and determine the type of exception.  If it's a simple type
// it calls memcpy.  It's a UDT with copy-ctor, it uses the copy-ctor.
//
void __ExceptionPtr::_CallCopyCtor(_Out_writes_bytes_(size) void* dst, _In_reads_bytes_(size) void* src, size_t size, _In_ const CatchableType * const pType) const
{
    if (CT_ISSIMPLETYPE(*pType) || // this is a simple type
            CT_COPYFUNC(*pType) == NULL    // this is a user defined type without copy ctor
       )
    {
        // just copy with memcpy
        memcpy(dst, src, size);

        if (CT_ISWINRTHANDLE(*pType))
        {
            IUnknown* pUnknown = *(reinterpret_cast<IUnknown**>(src));
            if (pUnknown)
            {
               pUnknown->AddRef();
            }
        }
    }
    else
    {
        try
        {
            // it's a user defined type with a copy ctor
            if (CT_HASVB(*pType))
            {
                // this exception got a virtual base
#if defined(_M_CEE_PURE)
                void (__clrcall *pFunc)(void *, void *, int) = (void (__clrcall *)(void *, void *, int))(void *)CT_COPYFUNC(*pType);
                pFunc((void *)dst, __AdjustPointer(src, CT_THISDISP(*pType)), 1);
#else
                _CallMemberFunction2((char *)dst,
                        CT_COPYFUNC(*pType),
                        __AdjustPointer(src,
                            CT_THISDISP(*pType)), 1);
#endif // defined(_M_CEE_PURE)
            }
            else
            {
#if defined(_M_CEE_PURE)
                void (__clrcall *pFunc)(void *, void *) = (void (__clrcall *)(void *, void *))(void *)CT_COPYFUNC(*pType);
                pFunc((void *)dst, __AdjustPointer(src, CT_THISDISP(*pType)));
#else
                _CallMemberFunction1((char *)dst,
                        CT_COPYFUNC(*pType),
                        __AdjustPointer(src,
                            CT_THISDISP(*pType)));
#endif // defined(_M_CEE_PURE)
            }    
        }
        catch(...)
        {
            // the copy-Ctor() threw.  We need to terminate().
            terminate();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::_CurrentException();
//
// Builds an __ExceptionPtr with the current exception inflight.  This function
// gets the information from the per-thead-data (PTD).
//
// This function returns the static copy of exception_ptr which points to a
// std::bad_alloc exception in out-of-memory situation.
//

shared_ptr<__ExceptionPtr> __ExceptionPtr::_CurrentException()
{

    // there is an exception in flight... copy it
    try
    {
        if (_pCurrentException == NULL || // no exception in flight
                __ProcessingThrow != 0 || // we are unwinding... possibly called from a dtor()
                ((_pCurrentException->ExceptionCode ==  MANAGED_EXCEPTION_CODE) ||
                (_pCurrentException->ExceptionCode ==  MANAGED_EXCEPTION_CODE_V4))    // we don't support managed exceptions
           )
        {
            // return a NULL exception_ptr
            return shared_ptr<__ExceptionPtr>();
        }

        shared_ptr<__ExceptionPtr> result(new __ExceptionPtr(_pCurrentException));
        return result;
    }
    catch(std::bad_alloc e)
    {
        return _BadAllocException();
    }
    catch (...)
    {
        // something went wrong, and it wasn't a bad_alloc and
        // we are not allowed to throw out of this function...
        terminate();
    }
}


template <typename T> class _DebugMallocator {
public:
    typedef T * pointer;
    typedef const T * const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    T * address(T& r) const {
        return &r;
    }

    const T * address(const T& s) const {
        return &s;
    }

    size_t max_size() const {
        return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
    }

    template <typename U> struct rebind {
        typedef _DebugMallocator<U> other;
    };

    bool operator!=(const _DebugMallocator& other) const {
        return !(*this == other);
    }

    void construct(T * const p, const T& t) const {
        void * const pv = static_cast<void *>(p);

        new (pv) T(t);
    }

    void destroy(T * const p) const;

    bool operator==(const _DebugMallocator& other) const {
        return true;
    }

    _DebugMallocator() { }

    _DebugMallocator(const _DebugMallocator&) { }

    template <typename U> _DebugMallocator(const _DebugMallocator<U>&) { }

    ~_DebugMallocator() { }

    T * allocate(const size_t n) const {
        if (n == 0) {
            return NULL;
        }

        if (n > max_size()) {
            throw std::length_error("_DebugMallocator<T>::allocate() - Integer overflow.");
        }

        void * const pv = _malloc_crt(n * sizeof(T));

        if (pv == NULL) {
            throw std::bad_alloc();
        }

        return static_cast<T *>(pv);
    }

    void deallocate(T * const p, size_t) const {
        _free_crt(p);
    }

    template <typename U> T * allocate(const size_t n, const U *) const {
        return allocate(n);
    }

private:
    _DebugMallocator& operator=(const _DebugMallocator&);
};

// A compiler bug causes it to believe that p->~T() doesn't reference p.

#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter

template <typename T> void _DebugMallocator<T>::destroy(T * const p) const {
    p->~T();
}

#pragma warning(pop)


void _DeleteExceptionPtr(__ExceptionPtr * const p) {
    _DebugMallocator<__ExceptionPtr> dm;
    dm.destroy(p);
    dm.deallocate(p, 1);
}


////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::_CopyException();
//
// Builds an __ExceptionPtr with the given c++ exception object and its corresponding
// throwinfo.
//
// This function returns the static copy of exception_ptr which points to a
// std::bad_alloc exception in out-of-memory situation.
//
shared_ptr<__ExceptionPtr> __ExceptionPtr::_CopyException(_In_ const void * pExceptObj, _In_ const ThrowInfo* pThrowInfo, bool normal)
{
    try
    {
        // the initial values of EHExceptionRecord need to be consistent with _CxxThrowException()
        EHExceptionRecord ehRecord;
        EHExceptionRecord* pExcept = &ehRecord; // just an alias for use of macros

        // initialize the record
        PER_CODE(pExcept) = EH_EXCEPTION_NUMBER;
        PER_FLAGS(pExcept) = EXCEPTION_NONCONTINUABLE;
        PER_NEXT(pExcept) = NULL; // no SEH to chain
        PER_ADDRESS(pExcept) = NULL; // Address of exception. Will be overwritten by OS
        PER_NPARAMS(pExcept) = EH_EXCEPTION_PARAMETERS;
        PER_MAGICNUM(pExcept) = EH_MAGIC_NUMBER1;
        PER_PEXCEPTOBJ(pExcept) = const_cast<void*>(pExceptObj);

        ThrowInfo* pTI = pThrowInfo;
        if (pTI && (THROW_ISWINRT( (*pTI) ) ) )
        {
            ULONG_PTR *exceptionInfoPointer = *reinterpret_cast<ULONG_PTR**>(const_cast<void*>(pExceptObj));
            exceptionInfoPointer--; // The pointer to the ExceptionInfo structure is stored sizeof(void*) infront of each WinRT Exception Info.

            WINRTEXCEPTIONINFO* wei = reinterpret_cast<WINRTEXCEPTIONINFO*>(*exceptionInfoPointer);
            pTI = wei->throwInfo;
        }

        PER_PTHROW(pExcept) = pTI;

#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)
        PVOID ThrowImageBase = RtlPcToFileHeader((PVOID)pTI, &ThrowImageBase);
        PER_PTHROWIB(pExcept) = ThrowImageBase;
#endif

        //
        // If the throw info indicates this throw is from a pure region,
        // set the magic number to the Pure one, so only a pure-region
        // catch will see it.
        //
        // Also use the Pure magic number on Win64 if we were unable to
        // determine an image base, since that was the old way to determine
        // a pure throw, before the TI_IsPure bit was added to the FuncInfo
        // attributes field.
        //
        if (pTI != NULL)
        {
            if (THROW_ISPURE(*pTI))
            {
                PER_MAGICNUM(pExcept) = EH_PURE_MAGIC_NUMBER1;
            }
#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)
            else if (ThrowImageBase == NULL)
            {
                PER_MAGICNUM(pExcept) = EH_PURE_MAGIC_NUMBER1;
            }
#endif
        }

        shared_ptr<__ExceptionPtr> result;

        if (normal)
        {
            result.reset(new __ExceptionPtr(pExcept));
        }
        else
        {
            _DebugMallocator<__ExceptionPtr> dm;

            void * pv = static_cast<void *>(dm.allocate(1));

            result.reset(new (pv) __ExceptionPtr(pExcept, false), _DeleteExceptionPtr, _DebugMallocator<int>());
        }

        return result;
    }
    catch(std::bad_alloc e)
    {
        return _BadAllocException();
    }
    catch (...)
    {
        // something went wrong, and it wasn't a bad_alloc and
        // we are not allowed to throw out of this function...
        terminate();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::__ExceptionPtr();
//
// Ctor for __ExceptionPtr.  This function copies the EXCEPTION_RECORD that is
// passed in.  If the EXCEPTION_RECORD is a C++ exception it will also, allocate
// heap memory for the C++ exception and call the copy-ctor for it.
//
// This function throws std::bad_alloc exception in out-of-memory situation.
//
__ExceptionPtr::__ExceptionPtr(_In_ const EHExceptionRecord * pExcept, bool normal)
{
    this->m_normal = normal;

    //copying the _EXCEPTION_RECORD (for SEH exceptions)
    PER_CODE(   &this->m_Record) = PER_CODE(pExcept);
    PER_FLAGS(  &this->m_Record) = PER_FLAGS(pExcept);
    PER_NEXT(   &this->m_Record) = 0; // We don't chain SEH exceptions
    PER_ADDRESS(&this->m_Record) = 0; // Useless field to copy.  It will be overwritten by RaiseException()
    PER_NPARAMS(&this->m_Record) = PER_NPARAMS(pExcept);
    // copying any addition parameters
    for (ULONG i = 0; i < this->m_Record.NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
    {
        this->m_Record.ExceptionInformation[i] = ((EXCEPTION_RECORD*)pExcept)->ExceptionInformation[i];
    }
    // NULLing out any un-used parameters
    for (ULONG i = this->m_Record.NumberParameters; i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
    {
        this->m_Record.ExceptionInformation[i] = 0;
    }

    if (PER_IS_MSVC_PURE_OR_NATIVE_EH(pExcept))
    {
        // this is a c++ exception
        // we need to copy the c++ exception object

        // nulling out the Exception object pointer, because we haven't copied it yet
        PER_PEXCEPTOBJ(&this->m_EHRecord) = NULL;

        ThrowInfo* pThrow= PER_PTHROW(pExcept);
        if (    PER_PEXCEPTOBJ(pExcept) == NULL ||
                pThrow == NULL ||
                pThrow->pCatchableTypeArray == NULL ||
                THROW_COUNT(*pThrow) <=0
                )
        {
            // No ThrowInfo exist.  If this were a c++ exception, something must have corrupted it.
            terminate();
        }
        // we want to encode the ThrowInfo pointer for security reasons
        PER_PTHROW(&this->m_EHRecord) = (ThrowInfo*) EncodePointer((void*)pThrow);

        // we finally got the type info we want
#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)
        CatchableType *const pType = (CatchableType * const)( *(int*)THROW_CTLIST_IB(*pThrow, _GetThrowImageBase()) + _GetThrowImageBase());
#else
        CatchableType *const pType = *THROW_CTLIST(*pThrow);
#endif

        void* pExceptionBuffer = NULL;

        if (normal)
        {
            pExceptionBuffer = malloc(CT_SIZE(*pType));
        }
        else
        {
            pExceptionBuffer = _malloc_crt(CT_SIZE(*pType));
        }

        if (pExceptionBuffer == NULL)
        {
            throw std::bad_alloc();
        }
        __ExceptionPtr::_CallCopyCtor(pExceptionBuffer, PER_PEXCEPTOBJ(pExcept), CT_SIZE(*pType),pType);

        PER_PEXCEPTOBJ(&this->m_EHRecord) = pExceptionBuffer;
    }
    //else // this is a SEH exception, no special handling is required
}

////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::~__ExceptionPtr();
//
// Dtor for __ExceptionPtr.  If the stored exception is a C++ exception, it destroys
// the C++ exception object and de-allocate the heap memory for it.
//
__ExceptionPtr::~__ExceptionPtr()
{
    EHExceptionRecord* pExcept = &this->m_EHRecord;

    if (PER_IS_MSVC_PURE_OR_NATIVE_EH(pExcept))
    {
        // this is a c++ exception
        // we need to delete the actual exception object
        ThrowInfo* pThrow= (ThrowInfo*) DecodePointer((void*)PER_PTHROW(pExcept));
        if ( pThrow == NULL )
        {
            // No ThrowInfo exist.  If this were a c++ exception, something must have corrupted it.
            terminate();
        }

        if (PER_PEXCEPTOBJ(pExcept) != NULL) // we have an object to destroy
        {
#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)
            CatchableType *const pType = (CatchableType * const)( *(int*)THROW_CTLIST_IB(*pThrow, _GetThrowImageBase()) + _GetThrowImageBase());
#else
            CatchableType *const pType = *THROW_CTLIST(*pThrow);
#endif        
            // calling the d-tor if there's one
            if (THROW_UNWINDFUNC(*pThrow) != NULL) // we have a dtor func                 
            {
                // it's a user defined type with a dtor
#if defined(_M_CEE_PURE)
                void (__clrcall * pDtor)(void*) =  (void (__clrcall *)(void *))(THROW_UNWINDFUNC(*pThrow));
                (*pDtor)(PER_PEXCEPTOBJ(pExcept));
#elif _EH_RELATIVE_OFFSETS
                _CallMemberFunction0(PER_PEXCEPTOBJ(pExcept),
                  THROW_UNWINDFUNC_IB(*pThrow,(unsigned __int64)PER_PTHROWIB(pExcept)));
#else
                _CallMemberFunction0(PER_PEXCEPTOBJ(pExcept),
                  THROW_UNWINDFUNC(*pThrow));
#endif
            }
            else if (CT_ISWINRTHANDLE(*pType))
            {
                IUnknown* pUnknown = *(reinterpret_cast<IUnknown**>(PER_PEXCEPTOBJ(pExcept)));
                if (pUnknown)
                {
                    pUnknown->Release();
                }
            }
        }

        // de-allocating the memory of the exception object
        if (this->m_normal)
        {
            free(PER_PEXCEPTOBJ(pExcept));
        }
        else
        {
            _free_crt(PER_PEXCEPTOBJ(pExcept));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// void __ExceptionPtr::_RethrowException() const;
//
// This function rethrows the exception that is stored in the exception_ptr object.
// If the stored exception is a SEH exception, it simply call RaiseException().
// If the stored exception is a C++ exception, it will try to allocate memory on
// the stack and copy the exception object onto the stack and calls RaiseException().
//
void __ExceptionPtr::_RethrowException() const
{
    // throwing a bad_exception if they give us a NULL exception_ptr
    if (this == NULL)
    {
        throw std::bad_exception();
    }

    EXCEPTION_RECORD ThisException = this->m_Record;
    EHExceptionRecord* pExcept = (EHExceptionRecord*)&ThisException; // just an alias for use of the macros

    if (PER_IS_MSVC_PURE_OR_NATIVE_EH(pExcept))
    {
        // this is a c++ exception
        // we need to build the exception on the stack
        // because the current exception mechanism assume
        // the exception object is on the stack and will call
        // the appropiate dtor (if there's one), but wont
        // delete the memory.
        ThrowInfo* pThrow= (ThrowInfo*) DecodePointer((void*)PER_PTHROW(pExcept));
        if (    PER_PEXCEPTOBJ(pExcept) == NULL ||
                pThrow == NULL ||
                pThrow->pCatchableTypeArray == NULL ||
                THROW_COUNT(*pThrow) <=0
                )
        {
            // No ThrowInfo exist.  If this were a c++ exception, something must have corrupted it.
            terminate();
        }
        PER_PTHROW(pExcept) = pThrow; // update the EXCEPTION_RECORD with the proper decoded pointer before rethrowing

        // we finally got the type info we want
#if _EH_RELATIVE_OFFSETS && !defined(_M_CEE_PURE)
        CatchableType *const pType = (CatchableType * const)( *(int*)THROW_CTLIST_IB(*pThrow, _GetThrowImageBase()) + _GetThrowImageBase());
#else
        CatchableType *const pType = *THROW_CTLIST(*pThrow);
#endif

        // alloc memory on stack for exception object
        // this might cause StackOverFlow SEH exception,
        // in that case, we just let the SEH propagate
        void * pExceptionBuffer = alloca(CT_SIZE(*pType));

        __ExceptionPtr::_CallCopyCtor(pExceptionBuffer, PER_PEXCEPTOBJ(pExcept), CT_SIZE(*pType), pType);
        PER_PEXCEPTOBJ(pExcept) = pExceptionBuffer;
    }
    //else // this is a SEH exception, no special handling is required

    // This shouldn't happen.  Just to make prefast/OACR happy
    if (ThisException.NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS)
    {
        ThisException.NumberParameters = EXCEPTION_MAXIMUM_PARAMETERS;
    }


    RaiseException( ThisException.ExceptionCode,
            ThisException.ExceptionFlags,
            ThisException.NumberParameters,
            ThisException.ExceptionInformation );
}

////////////////////////////////////////////////////////////////////////////////
//
// Tiny wrappers for bridging the gap between std::exception_ptr and shared_ptr<__ExceptionPtr>.
// We are doing this because <memory> depends on <exception> which means <exception> cannot include <memory>.
// And shared_ptr<> is defined <memory>.  To workaround this, we created a dummy class
// std::exception_ptr, which is structurally identical to shared_ptr<>.
//

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCreate(void* ptr)
{
    const shared_ptr<__ExceptionPtr>* buf = new (ptr) shared_ptr<__ExceptionPtr>();
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrDestroy(void* ptr)
{
    static_cast<shared_ptr<__ExceptionPtr>*>(ptr)->~shared_ptr<__ExceptionPtr>();
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCopy(void* _dst, const void* _src)
{
    shared_ptr<__ExceptionPtr>* dst = static_cast<shared_ptr<__ExceptionPtr>*>(_dst);
    const shared_ptr<__ExceptionPtr>* src = static_cast<const shared_ptr<__ExceptionPtr>*>(_src);
    new (dst) shared_ptr<__ExceptionPtr>(*src);
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrAssign(void* _dst, const void* _src)
{
    shared_ptr<__ExceptionPtr>* dst = static_cast<shared_ptr<__ExceptionPtr>*>(_dst);
    const shared_ptr<__ExceptionPtr>* src = static_cast<const shared_ptr<__ExceptionPtr>*>(_src);
    dst->operator=(*src);
}

_CRTIMP_PURE bool __CLRCALL_PURE_OR_CDECL __ExceptionPtrCompare(const void* _lhs, const void* _rhs)
{
    const shared_ptr<__ExceptionPtr>* lhs = static_cast<const shared_ptr<__ExceptionPtr>*>(_lhs);
    const shared_ptr<__ExceptionPtr>* rhs = static_cast<const shared_ptr<__ExceptionPtr>*>(_rhs);
    return operator==(*lhs, *rhs);
}

_CRTIMP_PURE bool __CLRCALL_PURE_OR_CDECL __ExceptionPtrToBool(const void* _ptr)
{
    const shared_ptr<__ExceptionPtr>* ptr = static_cast<const shared_ptr<__ExceptionPtr>*>(_ptr);
    return !!*ptr;
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrSwap(void* _lhs, void* _rhs)
{
    shared_ptr<__ExceptionPtr>* lhs = static_cast<shared_ptr<__ExceptionPtr>*>(_lhs);
    shared_ptr<__ExceptionPtr>* rhs = static_cast<shared_ptr<__ExceptionPtr>*>(_rhs);
    lhs->swap(*rhs);
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCurrentException(void* ptr)
{
    const shared_ptr<__ExceptionPtr> pExcept = __ExceptionPtr::_CurrentException();
    __ExceptionPtrCopy(ptr, static_cast<const void*>(&pExcept));
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrRethrow(const void* _ptr)
{
    const shared_ptr<__ExceptionPtr>* ptr = static_cast<const shared_ptr<__ExceptionPtr>*>(_ptr);
    (*ptr)->_RethrowException();
}

_CRTIMP_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCopyException(_Inout_ void* _ptr, _In_ const void* _pExcept, _In_ const void* _pThrowInfo)
{
    shared_ptr<__ExceptionPtr>* ptr = static_cast<shared_ptr<__ExceptionPtr>*>(_ptr);
    (*ptr) = __ExceptionPtr::_CopyException(_pExcept, static_cast<const ThrowInfo*>(_pThrowInfo));
}
