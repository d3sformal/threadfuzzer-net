#pragma once
#include "../net_types.hpp"
#include "../thread_id.hpp"
#include "../argument_data.hpp"

#include "../utils/byte_formatter.hpp"

#include <vector>
#include <atomic>
#include <string>
#include <memory_resource>
#include <ranges>

#include <Windows.h>

class thread_info
{
    thread_id thr_id;
    HANDLE thr_handle;

    std::atomic<bool> suspended;
    std::atomic<bool> marked_for_suspension;

public:
    std::unique_ptr<std::vector<const function_spec*>> call_stack;
    std::unique_ptr<std::vector<std::vector<argument_data>>> call_stack_args;

    explicit thread_info(thread_id thr_id)
        : thr_id(thr_id), thr_handle(OpenThread(THREAD_SUSPEND_RESUME, false, thr_id.native_id))
        , call_stack(new std::vector<const function_spec*>), call_stack_args(new std::vector<std::vector<argument_data>>)
    {
    }

    thread_info(const thread_info&) = delete;

    thread_info(thread_info&& other) noexcept // non thread safe
        : thr_id(other.thr_id), thr_handle(other.thr_handle)
        , call_stack(std::move(other.call_stack)), call_stack_args(std::move(other.call_stack_args))
    {
        other.thr_handle = nullptr;
    }

    thread_info& operator=(const thread_info&) = delete;

    thread_info& operator=(thread_info&& other) noexcept
    {
        thr_id = other.thr_id;
        thr_handle = other.thr_handle;
        other.thr_handle = nullptr;

        call_stack = std::move(other.call_stack);
        call_stack_args = std::move(other.call_stack_args);

        return *this;
    }

    ~thread_info()
    {
        if (thr_handle)
            CloseHandle(thr_handle);
    }

    [[nodiscard]] const thread_id& get_thread_id() const
    {
        return thr_id;
    }

    void freeze(bool stop_immediate = false)
    {
        if (suspended)
            return;

        if (!stop_immediate && thr_id.native_id != GetCurrentThreadId())
        {
            marked_for_suspension = true;
            return;
        }

        suspended = true;
        SuspendThread(thr_handle);
    }

    void thaw()
    {
        auto first_resume = ResumeThread(thr_handle);
        if (first_resume == 0)
            return;
        if (first_resume > 1)
        {
            while (ResumeThread(thr_handle) > 1)
                ;
        }
        marked_for_suspension = false;
        suspended = false;
    }

    bool is_frozen() const
    {
        return suspended.load();
    }

    bool is_marked_for_suspension() const
    {
        return marked_for_suspension.load();
    }

    std::wstring pretty_current_function() const
    {
        std::wstring str;
        pretty_current_function(str);
        return str;
    }

    template<typename Alloc>
    void pretty_current_function(std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc>& str) const
    {
        if (!call_stack || call_stack->empty())
        {
            str += L"NonStarted/Ended";
            return;
        }
        const auto* function = call_stack->back();
        str += function->get_pretty_info();

        if (suspended)
        {
            str += L" (";
            for (std::size_t i = function->is_static() ? 0 : 1; i < call_stack_args->back().size(); ++i)
            {
                const auto& arg_name = function->get_arguments()[i - (function->is_static() ? 0 : 1)]->get_name_simple();
                if (!args::dispatch([&str](auto arg_value) { std::format_to(std::back_inserter(str), L"{}, ", arg_value); }, call_stack_args->back().at(i), arg_name))
                    str += L"UnknownValue, ";
            }

            if (call_stack_args->back().size() > (function->is_static() ? 0u : 1u))
            {
                str.pop_back();
                str.pop_back();
            }
            str += L")";
        }
    }
};

namespace views
{
    static constexpr auto as_thread_info_ptrs = std::views::filter([](const std::optional<thread_info>& thr_info_opt) { return thr_info_opt.has_value(); })
        | std::views::transform([](std::optional<thread_info>& thr_info_opt) -> thread_info*
        {
            try
            {
                return &thr_info_opt.value();
            }
            catch (std::bad_optional_access&)
            {
                return nullptr;
            }
        });
    static constexpr auto without_self = std::views::filter([](const thread_info* thr_info) { return thr_info->get_thread_id().native_id != GetCurrentThreadId(); });
    static constexpr auto only_frozen = std::views::filter([](const thread_info* thr_info) { return thr_info->is_frozen(); });
}
