#pragma once

#include "thread_info.hpp"
#include "stop_points.hpp"
#include "trace.hpp"
#include "atomic_value_exchanger.hpp"
#include "thread_preemption_bound.hpp"

#include "../net_types.hpp"
#include "../argument_data.hpp"
#include "../cor_profiler.hpp"
#include "../thread_id.hpp"

#include "../utils/heap_allocating_resource.hpp"
#include "../utils/console.hpp"
#include "../utils/spin_lock.hpp"

#include <shared_mutex>
#include <mutex>
#include <ranges>
#include <regex>
#include <atomic>
#include <thread>
#include <vector>
#include <memory_resource>
#include <iostream>
#include <fstream>

#include <Windows.h>

static bool regex_matches(const std::wregex& regex, const std::wstring& str)
{
    return std::regex_match(str, regex);
}

template <typename Driver>
class thread_controller
{
    const cor_profiler& profiler;
    std::atomic<bool> is_enabled;
    bool stop_immediate;

    DWORD main_thread_id;

    std::pmr::memory_resource* memory_resource;
    std::pmr::vector<std::optional<thread_info>> thread_infos;
    std::mutex thread_infos_mtx;

    std::thread loop_thread;

    stop_points weak_points;
    stop_points strong_points;

    thread_preemption_bound thread_preemption_bound;

    atomic_value_exchanger<thread_info> thread_info_to_add;
    atomic_value_exchanger<std::reference_wrapper<thread_info>> thread_info_to_remove;

    spin_lock spin_lock;

    thread_info* get_thread_info()
    {
        auto counted_id = profiler.get_thread_id(GetCurrentThreadId()).counted_id;
        if (counted_id < thread_infos.size() && thread_infos.at(counted_id).has_value())
            return &thread_infos.at(counted_id).value();
        return nullptr;
    }

    thread_info* register_thread_info(thread_info&& thr_info)
    {
        auto counted_id = thr_info.get_thread_id().counted_id;

        thread_info_to_add.store(std::move(thr_info));

        return &thread_infos[counted_id].value();
    }

    void deregister_thread_info(thread_info& thr_info)
    {
        thread_info_to_remove.store(thr_info);
    }

    static std::function<void()> init_sleep_function(std::chrono::microseconds duration)
    {
        static auto long_sleep = [duration]
        {
            std::this_thread::sleep_for(duration);
        };

        static auto yield_sleep = [duration]
        {
            auto start = std::chrono::system_clock::now();
            while (start + duration > std::chrono::system_clock::now())
                std::this_thread::yield();
        };

        static auto active_sleep = [duration]
        {
            auto start = std::chrono::system_clock::now();
            while (start + duration > std::chrono::system_clock::now());
        };

        static auto single_yield = []
        {
            std::this_thread::yield();
        };

        static auto nop = [] {};

        if (duration.count() > 25000)
            return long_sleep;

        if (duration.count() > 50)
            return yield_sleep;

        if (duration.count() > 0)
            return active_sleep;

        if (duration.count() == 0)
            return single_yield;

        return nop;
    }

    void thread_controller_loop(Driver&& driver)
    try
    {
        const std::wstring& text_trace_file = config_file::get_instance().get_value(L"trace_file");
        std::wostream* output = nullptr;
        std::unique_ptr<std::wofstream> fstream_ptr;
        if (text_trace_file == L"-")
            output = &std::wcout;
        else if (text_trace_file == L"--")
            output = &std::wcerr;
        else if (!text_trace_file.empty())
        {
            fstream_ptr = std::make_unique<std::wofstream>(text_trace_file);
            output = fstream_ptr.get();
        }

        bool trace_enabled = output;
        bool data_file_enabled = !config_file::get_instance().get_value(L"data_file").empty() && driver.should_update_data_file();

        std::chrono::microseconds thawing_timeout(config_file::get_instance().get_value<int>(L"thawing_timeout"));
        auto sleep_func = init_sleep_function(thawing_timeout);

        trace trace(memory_resource);
        while (is_enabled)
        {
            if (auto [thr_info, raii_release] = thread_info_to_add.load(); thr_info != nullptr)
            {
                while (thr_info->get_thread_id().counted_id >= thread_infos.size())
                    thread_infos.emplace_back();

                thread_infos[thr_info->get_thread_id().counted_id] = std::move(*thr_info);
            }

            if (auto [thr_info_ref, raii_release] = thread_info_to_remove.load(); thr_info_ref != nullptr)
            {
                auto* thr_info = &thr_info_ref->get();

                spin_lock.lock();

                if (thr_info->is_frozen())
                    thr_info->thaw();

                thread_infos[thr_info->get_thread_id().counted_id].reset();

                spin_lock.unlock();
            }

            if (!std::ranges::empty(thread_infos | views::as_thread_info_ptrs | views::only_frozen))
            {
                sleep_func();
                auto threads = driver.threads_to_run(thread_infos | views::as_thread_info_ptrs, trace);
                if (threads.empty() || std::ranges::all_of(threads, [](const thread_info* thr_info) { return thr_info == nullptr; }))
                    continue;

                std::size_t options_size = std::ranges::distance(thread_infos | views::as_thread_info_ptrs | views::only_frozen);

                if (trace_enabled || data_file_enabled)
                    trace.add(threads, options_size);

                for (thread_info* thr_info : threads)
                {
                    if (thr_info->is_frozen())
                        thr_info->thaw();
                }
            }
            else
                std::this_thread::yield();
        }

        if (data_file_enabled)
        {
            trace_file trace_log(config_file::get_instance().get_value(L"data_file"));
            trace_log.append_trace(trace);
        }

        if (output)
        {
            *output << std::endl;
            trace.for_each([output, this](const trace_item& item)
            {
                *output << item.thr_id.counted_id << L" " << item.function->get_pretty_info(memory_resource) << L" [options: " << item.options_size << L"]" << std::endl;
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // Some threads might be still frozen when entry_point leaves if there are some detached threads still running
        for (auto* thr_info : thread_infos | views::as_thread_info_ptrs)
            if (thr_info->is_frozen())
                thr_info->thaw();
    }
    catch (const profiler_error& err)
    {
        profiler.log<logging_level::ERROR>(err.wwhat());
    }

    std::thread create_debugger_loop_thread()
    {
        std::thread thread(&thread_controller::thread_controller_loop, this, Driver{ profiler, memory_resource });
        SetThreadDescription(thread.native_handle(), L"DebuggerLoopThread");

        return thread;
    }

public:
    thread_controller(const cor_profiler& profiler, stop_points&& weak_points, stop_points&& strong_points, ::thread_preemption_bound&& thread_preemption_bound, bool stop_immediate)
        : profiler(profiler), is_enabled(false), stop_immediate(stop_immediate), main_thread_id(-1)
        , memory_resource(stop_immediate ? new heap_allocating_resource : std::pmr::get_default_resource())
        , thread_infos(memory_resource)
        , weak_points(std::move(weak_points)), strong_points(std::move(strong_points))
        , thread_preemption_bound(std::move(thread_preemption_bound))
    {
        profiler.set_function_entry_hook([this](const function_spec* function, const std::vector<argument_data>& args)
        {
            this->method_entry(function, args);
        });

        profiler.set_function_leave_hook([this](const function_spec* function)
        {
            this->method_leave(function);
        });

        thread_infos.reserve(1024);
    }

    void method_entry(const function_spec* function, const std::vector<argument_data>& args)
    {
        if (!is_enabled && profiler.is_entry_point(function))
        {
            is_enabled = true;
            main_thread_id = GetCurrentThreadId();

            loop_thread = create_debugger_loop_thread();
            profiler.log<logging_level::INFO>(L"Enabled thread_control: Good Luck [id: ", GetThreadId(loop_thread.native_handle()), "]");
        }

        if (!is_enabled)
            return;

        auto* thr_info = get_thread_info();
        if (!thr_info)
        {
            thread_info new_thr_info(profiler.get_thread_id(GetCurrentThreadId()));
            new_thr_info.call_stack_args->push_back(args);
            new_thr_info.call_stack->push_back(function);
            thr_info = register_thread_info(std::move(new_thr_info));
        }
        else
        {
            thr_info->call_stack_args->push_back(args);
            thr_info->call_stack->push_back(function);
        }

        if (thread_preemption_bound.current < thread_preemption_bound.max)
        {
            if (strong_points.matches(*thr_info->call_stack))
            {
                ++thread_preemption_bound.current;

                spin_lock.lock();
                for (auto& thr_info_opt : thread_infos)
                {
                    // using ranges/views here sometimes overruns the thread_infos and corrupts memory
                    if (thr_info_opt.has_value() && thr_info_opt->get_thread_id().native_id != GetCurrentThreadId())
                        thr_info_opt->freeze(stop_immediate);
                }
                spin_lock.unlock();

                thr_info->freeze(stop_immediate);
            }
            else if (thr_info->is_marked_for_suspension() || weak_points.matches(*thr_info->call_stack))
            {
                ++thread_preemption_bound.current;
                thr_info->freeze(stop_immediate);
            }
        }
        else if (thread_preemption_bound.strategy == thread_preemption_bound_strategy::EXIT)
        {
            ExitProcess(0);
        }
    }

    void method_leave(const function_spec* function)
    {
        if (!is_enabled)
            return;

        auto* thr_info = get_thread_info();

        if (thr_info->is_marked_for_suspension())
            thr_info->freeze(stop_immediate);

        if (thr_info->call_stack->size() == 1) // last function
        {
            thr_info->call_stack.reset();
            thr_info->call_stack_args.reset();
            deregister_thread_info(*thr_info);
        }
        else
        {
            thr_info->call_stack->pop_back();
            thr_info->call_stack_args->pop_back();
        }

        if (profiler.is_entry_point(function))
        {
            is_enabled = false;
            profiler.log<logging_level::INFO>(L"Disabled thread_control");
            loop_thread.join();
        }
    }
};
