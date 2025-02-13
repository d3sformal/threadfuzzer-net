// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// Modified
#pragma once

#include "cor_error_handling.hpp"
#include "argument_data.hpp"
#include "config_file.hpp"

#include <cor.h>
#include <corprof.h>
#include <winerror.h>

#include <direct.h>
#include <limits.h>

#include <cstdlib>
#include <atomic>

extern "C" void __stdcall functionEnterC(UINT_PTR, COR_PRF_ELT_INFO);
extern "C" void __stdcall functionLeaveC(UINT_PTR, COR_PRF_ELT_INFO);
extern "C" HRESULT __stdcall stackSnapshotC(FunctionID, UINT_PTR, COR_PRF_FRAME_INFO, ULONG32, BYTE[], void*);
extern "C" UINT_PTR __stdcall functionIdMapper(FunctionID, void*, BOOL*);

#ifdef _M_IX86 // x86
inline __declspec(naked) void __stdcall functionEnterCallback(FunctionIDOrClientID functionId, COR_PRF_ELT_INFO eltInfo)
{
    __asm {
        push ebp
        mov  ebp, esp
        pushad
    }

    functionEnterC(functionId.clientID, eltInfo);

    __asm {
        popad
        pop  ebp
        ret  SIZE functionId + SIZE eltInfo
    }
}

inline __declspec(naked) void __stdcall functionLeaveCallback(FunctionIDOrClientID functionId, COR_PRF_ELT_INFO eltInfo)
{
    __asm {
        push ebp
        mov  ebp, esp
        pushad
    }

    functionLeaveC(functionId.clientID, eltInfo);

    __asm {
        popad
        pop  ebp
        ret  SIZE functionId + SIZE eltInfo
    }
}
#endif

class cor_profiler_base : public ICorProfilerCallback8
{
    std::atomic<int> ref_count;
protected:
    ICorProfilerInfo8* cor_profiler_info;

public:
    cor_profiler_base() : ref_count(0), cor_profiler_info(nullptr)
    {
    }

    cor_profiler_base(const cor_profiler_base&) = delete;
    cor_profiler_base& operator=(const cor_profiler_base&) = delete;

    virtual ~cor_profiler_base() = default;

    HRESULT STDMETHODCALLTYPE Initialize(IUnknown* pICorProfilerInfoUnk) override
    {
        if (pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo8), reinterpret_cast<void**>(&this->cor_profiler_info)) < 0)
            return E_FAIL;

        DWORD event_mask = COR_PRF_MONITOR_JIT_COMPILATION
            | COR_PRF_DISABLE_TRANSPARENCY_CHECKS_UNDER_FULL_TRUST /* helps the case where this profiler is used on Full CLR */
            | COR_PRF_DISABLE_INLINING
            | COR_PRF_MONITOR_THREADS
            | COR_PRF_MONITOR_CLASS_LOADS
         // | COR_PRF_MONITOR_OBJECT_ALLOCATED
            | COR_PRF_ENABLE_OBJECT_ALLOCATED
            | COR_PRF_MONITOR_ENTERLEAVE
            | COR_PRF_ENABLE_FUNCTION_ARGS
            | COR_PRF_ENABLE_FUNCTION_RETVAL
            | COR_PRF_ENABLE_FRAME_INFO
            | COR_PRF_ENABLE_STACK_SNAPSHOT
            | COR_PRF_MONITOR_EXCEPTIONS
            ;

        if (this->cor_profiler_info->SetEventMask(event_mask) < 0)
            return E_FAIL;

        if (this->cor_profiler_info->SetEnterLeaveFunctionHooks3WithInfo(&functionEnterCallback, &functionLeaveCallback, nullptr) != S_OK)
            return E_FAIL;

        if (this->cor_profiler_info->SetFunctionIDMapper2(&functionIdMapper, nullptr) != S_OK)
            return E_FAIL;

        ULONG length_offset;
        ULONG buffer_offset;
        COR_SANITIZE(cor_profiler_info->GetStringLayout2(&length_offset, &buffer_offset));
        argument_data::set_string_layout(length_offset, buffer_offset);

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Shutdown() override
    {
        if (this->cor_profiler_info != nullptr)
        {
            this->cor_profiler_info->Release();
            this->cor_profiler_info = nullptr;
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv_object) override
    {
        if (riid == __uuidof(ICorProfilerCallback8) ||
            riid == __uuidof(ICorProfilerCallback7) ||
            riid == __uuidof(ICorProfilerCallback6) ||
            riid == __uuidof(ICorProfilerCallback5) ||
            riid == __uuidof(ICorProfilerCallback4) ||
            riid == __uuidof(ICorProfilerCallback3) ||
            riid == __uuidof(ICorProfilerCallback2) ||
            riid == __uuidof(ICorProfilerCallback) ||
            riid == __uuidof(IUnknown)
        )
        {
            *ppv_object = this;
            this->AddRef();
            return S_OK;
        }

        *ppv_object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++ref_count;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG count = --ref_count;

        if (count <= 0)
            delete this;

        return count;
    }
};
