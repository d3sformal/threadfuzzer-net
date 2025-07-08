// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// Modified

#pragma once
#include "cor_profiler.hpp"
#include "config_file.hpp"

#include "thread_interleaving_control/thread_controller.hpp"
#include "thread_interleaving_control/stop_points.hpp"

#include "thread_interleaving_control/fuzzing_driver.hpp"
#include "thread_interleaving_control/console_driver.hpp"
#include "thread_interleaving_control/systematic_driver.hpp"
#include "thread_interleaving_control/pursuing_driver.hpp"

#include <atomic>

inline cor_profiler* profiler = nullptr;
inline void* thr_debugger;

class class_factory : public IClassFactory
{
    std::atomic<int> ref_count;
public:
    class_factory() : ref_count(0)
    {
    }

    virtual ~class_factory() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv_object) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory))
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
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* unk_outer, REFIID riid, void** ppv_object) override
    {
        if (unk_outer != nullptr)
        {
            *ppv_object = nullptr;
            return CLASS_E_NOAGGREGATION;
        }

        try
        {
            char* config_path = nullptr;
            std::size_t unused;
            COR_SANITIZE(_dupenv_s(&config_path, &unused, "COR_PROFILER_CONFIG_FILE"));
            std::unique_ptr<char, decltype(free)*> storage{ config_path, free };
            if (config_path != nullptr)
                config_file::load_file(config_path);
            else
                throw profiler_error(L"COR_PROFILER_CONFIG_FILE not set");

            auto& config = config_file::get_instance();

            stop_points weak_points(config.get_values(L"weak_points"));
            stop_points strong_points(config.get_values(L"strong_points"));

            std::size_t thread_preemption_bound = !config.get_value(L"thread_preemption_bound").empty()
                ? config.get_value<int>(L"thread_preemption_bound")
                : static_cast<std::size_t>(-1);

            const std::wstring& debug_type = config.get_value(L"debug_type");
            bool stop_immediate = config.get_value(L"stop_type") == L"immediate";

            profiler = new cor_profiler();

            if (debug_type == L"console")
                thr_debugger = new thread_controller<console_driver>(*profiler, std::move(weak_points), std::move(strong_points), thread_preemption_bound, stop_immediate);
            else if (debug_type == L"fuzzing")
                thr_debugger = new thread_controller<fuzzing_driver>(*profiler, std::move(weak_points), std::move(strong_points), thread_preemption_bound, stop_immediate);
            else if (debug_type == L"systematic")
            {
                if (config.get_value(L"data_file").empty())
                    throw profiler_error(L"Debug type 'systematic' requires 'data_file'.");
                thr_debugger = new thread_controller<systematic_driver>(*profiler, std::move(weak_points), std::move(strong_points), thread_preemption_bound, stop_immediate);
            }
            else if (debug_type == L"pursuing")
            {
                if (config.get_value(L"data_file").empty())
                    throw profiler_error(L"Debug type 'pursuing' requires 'data_file'.");
                thr_debugger = new thread_controller<pursuing_driver>(*profiler, std::move(weak_points), std::move(strong_points), thread_preemption_bound, stop_immediate);
            }
            else
                throw profiler_error(L"Invalid 'debug_type'");
        }
        catch (const profiler_error& err)
        {
            if (profiler)
                profiler->log<logging_level::ERROR>(err.wwhat());
            else
                std::wcout << err.wwhat() << std::endl;

            delete profiler;
            return E_FAIL;
        }

        return profiler->QueryInterface(riid, ppv_object);
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override
    {
        return S_OK;
    }
};

