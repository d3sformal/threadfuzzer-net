#pragma once

#include "driver_base.hpp"
#include "../thread_info.hpp"
#include "../trace.hpp"
#include "../thread_preemption_bound.hpp"

#include "../../config_file.hpp"

#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>

class pursuing_driver : public driver_base
{
    std::vector<past_trace_item> pursuing_trace;
    std::size_t cur_index;

public:
    pursuing_driver(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource, const ::thread_preemption_bound& tpb)
        : driver_base(profiler, mem_resource, tpb)
        , cur_index(0)
    {
        auto path = config_file::get_instance().get_value(L"data_file");
        if (std::filesystem::is_empty(path))
            throw profiler_error(L"No data file for pursuing driver.");

        auto trace_index = 0; // ToDo: select which

        trace_file trace_log(path);
        trace_log.get_trace(trace_index, pursuing_trace);
    }

    template<std::ranges::range ThreadInfosRange>
    std::pmr::vector<thread_info*> threads_to_run(ThreadInfosRange&& thread_infos, const trace& trace)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        std::pmr::vector<thread_info*> result(get_memory_resource());

        std::size_t current_index = cur_index++;
        if (current_index < pursuing_trace.size())
        {
            for (thread_info* thr_info : thread_infos | views::only_frozen)
            {
                if (pursuing_trace[current_index].counted_id == thr_info->get_thread_id().counted_id)
                {
                    if (!thr_info->call_stack || pursuing_trace[current_index].function_id.compare(thr_info->call_stack->back()->get_pretty_info(get_memory_resource())))
                    {
                        // can't log, as logging locks a mutex for thread safe io
                        // log_async<logging_level::VERBOSE>(L"Stage ", current_index, ": Complete match");

                        result.push_back(thr_info);
                        return result;
                    }

                    // log_async<logging_level::VERBOSE>(L"Stage ", current_index, ": Partial match");
                    result.push_back(thr_info);
                    return result;
                }
            }
        }
        // log_async<logging_level::VERBOSE>(L"Stage ", current_index, ": No match");
        result.push_back((thread_infos | views::only_frozen).front());
        return result;
    }

    static bool should_update_data_file()
    {
        return false;
    }
};
