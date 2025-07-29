#pragma once

#include "thread_info.hpp"
#include "trace.hpp"
#include "driver_base.hpp"

#include "../utils/console.hpp"
#include "../utils/wstring_split.hpp"

#include <vector>
#include <thread>
#include <chrono>
#include <format>

class console_driver : public driver_base
{
    console console; // Hopefully the allocation happens before some threads could get to strong point (otherwise make this into allocator aware type)

public:
    console_driver(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource)
        : driver_base(profiler, mem_resource)
    {
    }

    template<std::ranges::range ThreadInfosRange>
    std::pmr::vector<thread_info*> threads_to_run(ThreadInfosRange&& thread_infos, const trace&)
    {
        return threads_to_run(std::forward<ThreadInfosRange>(thread_infos));
    }

    template<std::ranges::range ThreadInfosRange>
    std::pmr::vector<thread_info*> threads_to_run(ThreadInfosRange&& thread_infos)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(40)); // helps with letting functions run to stop points

        std::pmr::wstring str(get_memory_resource());
        str += L"Current threads\n";

        for (const thread_info* thr_info : thread_infos)
        {
            std::format_to(std::back_inserter(str), L"   [{}, thr: {:5}] {} at ",
                thr_info->get_thread_id().counted_id, thr_info->get_thread_id().native_id,
                thr_info->is_frozen() ? L" Frozen" : L"Running"
            );
            thr_info->pretty_current_function(str);
            /*
            if (thr_info->is_frozen())
            {
                auto stack_info = get_profiler().get_stack_info(get_memory_resource(), thr_info->get_thread_id().managed_id); // may invoke register_function with is not lock-free
                if (stack_info.size() > 0)
                    std::format_to(std::back_inserter(str), L", ILOff: {}", stack_info.back().get_il_offset());
                else
                    std::format_to(std::back_inserter(str), L", ILOff: unk");
            }
            */

            str += L"\n";
        }
        str += L"Option? ";

        console.print(str);
        auto line = console.getline();

        std::pmr::vector<thread_info*> thr_infos(get_memory_resource());
        if (line.empty())
            return thr_infos;

        static std::wregex delimiter { L", *" };
        auto thr_ids = tmt::pmr::wstring_split(get_memory_resource(), line, delimiter);

        for (const std::wstring& thr_id : thr_ids)
        {
            auto thr_info_iter = std::ranges::find_if(thread_infos, [&thr_id](const thread_info* thr_info)
            {
                return thr_info->get_thread_id().counted_id == static_cast<std::size_t>(std::stoi(thr_id));
            });
            if (thr_info_iter != std::ranges::end(thread_infos))
                thr_infos.push_back(*thr_info_iter);
        }

        return thr_infos;
    }

    static bool should_update_data_file()
    {
        return true;
    }
};
