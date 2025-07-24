#pragma once

#include "thread_info.hpp"
#include "trace.hpp"
#include "driver_base.hpp"

#include "../config_file.hpp"
#include "../thread_safe_logger.hpp"
#include "../utils/tree_graph.hpp"

#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <chrono>
#include <iostream>

template<typename TreePruner>
class systematic_driver : public driver_base
{
    enum class search_type_t : std::uint8_t
    {
        FIRST,
        LAST,
        RANDOM,
    };

    trace_file trace_file;
    pmr::tree_graph<int, past_trace_item> call_graph;
    const decltype(call_graph)::graph_vertex* current_vertex;

    std::size_t seed;
    std::mt19937 rng_engine;

    std::wofstream extra_log;
    std::chrono::steady_clock::time_point profiler_start;
    search_type_t search_type;

    template<typename... Ts>
    void log(Ts&&... ts)
    {
        if (!extra_log)
            return;

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - profiler_start).count();

        std::pmr::wstring prefix(get_memory_resource());
        std::format_to(std::back_inserter(prefix), L"[{:10}] [{:5}] [{}] ", duration, GetCurrentThreadId(), L"Info");

        extra_log << prefix;
        (extra_log << ... << ts);
        extra_log << std::endl;
    }

    [[nodiscard]] std::pmr::wstring format_thread_info(const thread_info* thr_info) const
    {
        std::pmr::wstring thr_info_str(get_memory_resource());
        std::format_to(std::back_inserter(thr_info_str), L"({}, {})", thr_info->get_thread_id().counted_id, thr_info->call_stack ? thr_info->call_stack->back()->get_pretty_info(get_memory_resource()).c_str() : L"UNK");
        return thr_info_str;
    }

    [[nodiscard]] std::pmr::wstring format_vertex(decltype(current_vertex) vertex) const
    {
        std::pmr::wstring vertex_str(get_memory_resource());
        if (vertex)
            std::format_to(std::back_inserter(vertex_str), L"[{:x} S{} ({})]", reinterpret_cast<std::uint32_t>(vertex), vertex->edges_size(), vertex->value);
        else
            vertex_str += L"nullptr";
        return vertex_str;
    }

    [[nodiscard]] std::pmr::wstring format_edge(const past_trace_item& edge) const
    {
        std::pmr::wstring edge_str(get_memory_resource());
        std::format_to(std::back_inserter(edge_str), L"{} {} {}", edge.counted_id, edge.function_id, edge.options_size);
        return edge_str;
    }

    static search_type_t get_search_type(const std::wstring& type)
    {
        if (type == L"first")
            return search_type_t::FIRST;
        if (type == L"last")
            return search_type_t::LAST;
        if (type == L"random")
            return search_type_t::RANDOM;
        throw profiler_error(L"Unknown search_type strategy");
    }

public:
    systematic_driver(const systematic_driver&) = delete;

    systematic_driver(systematic_driver&& other) noexcept
        : driver_base(other.get_profiler(), other.get_memory_resource())
        , trace_file(std::move(other.trace_file))
        , call_graph(std::move(other.call_graph))
        , current_vertex(&call_graph.root())
        , seed(other.seed), rng_engine(other.rng_engine)
        , extra_log(std::move(other.extra_log)), profiler_start(other.profiler_start)
        , search_type(other.search_type)
    {
    }

    systematic_driver& operator=(const systematic_driver& other) = delete;
    systematic_driver& operator=(systematic_driver&& other) = delete;
    ~systematic_driver() override = default;

    systematic_driver(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource, std::size_t seed = std::random_device{}())
        : driver_base(profiler, mem_resource)
        , trace_file(config_file::get_instance().get_value(L"data_file"))
        , call_graph(mem_resource), current_vertex(&call_graph.root())
        , seed(seed), rng_engine(seed)
        , search_type(search_type_t::FIRST)
    {
        if (!trace_file)
            throw profiler_error(L"Invalid data_file");

        auto& params = config_file::get_instance().get_values(L"debug_type");
        if (params.size() > 1)
            search_type = get_search_type(params[1]);
        if (params.size() > 2)
        {
            extra_log = std::wofstream(params[2], std::ios::app);
            extra_log << L"[ NEW ITERATION (search_type=" << params[1] << L") ]" << std::endl;
        }

        trace_file.populate_call_graph(call_graph, mem_resource);

        TreePruner::prune_tree(call_graph, extra_log);

        if (call_graph.root().edges_size() == 0)
        {
            // First run, let it run to get something
            current_vertex = nullptr;
        }
        else if (call_graph.root().value == 0)
            throw profiler_error(L"Exhausted options for systematic_driver");
    }

    template<std::ranges::range ThreadInfosRange>
    std::pmr::vector<thread_info*> threads_to_run(ThreadInfosRange&& thread_infos, const trace& trace)
    {
        std::pmr::vector<thread_info*> result(get_memory_resource());
        result.push_back(single_thread_to_run(std::forward<ThreadInfosRange>(thread_infos), trace));
        return result;
    }

    static bool should_update_data_file()
    {
        return true;
    }

private:
    template<std::ranges::range ThreadInfosRange>
    thread_info* single_thread_to_run(ThreadInfosRange&& thread_infos, const trace& trace)
    {
        log(L"-- Selecting next thread --");
        log(L"  Options:");
        for (auto* thr_info : thread_infos | views::only_frozen)
            log(L"    - ", format_thread_info(thr_info));
        log(L"  Call graph before:");
        auto father = current_vertex ? current_vertex->previous_vertex() : nullptr;
        log(L"    Father: ", format_vertex(father));
        log(L"    Current: ", format_vertex(current_vertex));
        if (current_vertex)
            for (std::size_t i = 0; i < current_vertex->edges_size(); ++i)
            {
                std::pmr::wstring line(get_memory_resource());
                std::format_to(std::back_inserter(line), L"    [{}] {} ({})", i, format_vertex(&current_vertex->next_vertex(i)), format_edge(current_vertex->get_edge(i)));
                log(line);
            }

        auto* thr_info = single_thread_to_run_impl(std::forward<ThreadInfosRange>(thread_infos), trace);
        log(L"  Updated vertex: ", format_vertex(current_vertex));
        log(L"  Result: ", format_thread_info(thr_info));
        extra_log << std::endl;

        return thr_info;
    }

    template<std::ranges::range Range>
    std::ranges::range_value_t<Range> select_value(Range&& range)
    {
        std::ranges::range_value_t<Range> value;
        if (search_type == search_type_t::RANDOM)
            std::ranges::sample(range, &value, 1, rng_engine);
        else if (search_type == search_type_t::FIRST)
            value = range.front();
        else if (search_type == search_type_t::LAST)
            value = range.back();
        else
            throw profiler_error(L"Unknown search_type strategy");
        return value;
    }

    template<std::ranges::range ThreadInfosRange>
    thread_info* single_thread_to_run_impl(ThreadInfosRange&& thread_infos, const trace& trace)
    {
        if (!current_vertex)
        {
            // unexplored path, pick one
            log(L"  Reason: No current vertex");
            return select_value(thread_infos | views::only_frozen);
        }

        // get all nonexhausted vertices
        std::pmr::vector<std::size_t> indices(get_memory_resource());
        for (std::size_t i = get_next_nonexhausted_vertex_index(0); i < current_vertex->edges_size(); i = get_next_nonexhausted_vertex_index(i + 1))
            indices.push_back(i);

        // select one
        if (!indices.empty())
        {
            std::size_t random_index = select_value(indices);
            const auto& edge = current_vertex->get_edge(random_index);
            for (thread_info* thr_info : thread_infos | views::only_frozen)
            {
                if (trace_matches_edge(*thr_info, edge))
                {
                    current_vertex = &current_vertex->next_vertex(random_index);
                    std::pmr::wstring line(get_memory_resource());
                    std::format_to(std::back_inserter(line), L"  Reason: Random nonexhausted vertex, matched edge [{}]", random_index);
                    log(line);
                    return thr_info;
                }
            }
        }

        // try to pick non-explored, but previously seen path first
        std::size_t index = get_next_nonexhausted_vertex_index(0);
        while (index < current_vertex->edges_size())
        {
            const auto& edge = current_vertex->get_edge(index);
            for (thread_info* thr_info : thread_infos | views::only_frozen)
            {
                if (trace_matches_edge(*thr_info, edge))
                {
                    current_vertex = &current_vertex->next_vertex(index);
                    std::pmr::wstring line(get_memory_resource());
                    std::format_to(std::back_inserter(line), L"  Reason: Iterating over non-exhausted vertex, matched edge [{}]", index);
                    log(line);
                    return thr_info;
                }
            }

            // Not in this edge, pick next
            index = get_next_nonexhausted_vertex_index(index + 1);
        }

        // None direct continuation, select new one that doesn't match
        for (thread_info* thr_info : thread_infos | views::only_frozen)
        {
            if (!thr_info_matches_any_edge(*thr_info))
            {
                current_vertex = nullptr;
                log(L"  Reason: Fallback, exploring new path");
                return thr_info;
            }
        }

        // all edges matches, but we except new path available, i.e. some path was available in previous run but is not here anymore. Just let run
        current_vertex = nullptr;

        log(L"  Reason: Unknown area, what happened?");
        return (thread_infos | views::only_frozen).front();
    }

    [[nodiscard]] bool trace_matches_edge(const thread_info& thr_info, const past_trace_item& item) const
    {
        return item.counted_id == thr_info.get_thread_id().counted_id
            && item.function_id.compare(thr_info.call_stack->back()->get_pretty_info(get_memory_resource())) == 0 // NOLINT(readability-string-compare)
            ;
    }

    [[nodiscard]] std::size_t get_next_nonexhausted_vertex_index(std::size_t start_index) const
    {
        for (std::size_t i = start_index; i < current_vertex->edges_size(); ++i)
        {
            if (current_vertex->next_vertex(i).value != 0)
                return i;
        }
        return current_vertex->edges_size();
    }

    [[nodiscard]] bool thr_info_matches_any_edge(const thread_info& thr_info) const
    {
        for (std::size_t i = 0; i < current_vertex->edges_size(); ++i)
        {
            const auto& edge = current_vertex->get_edge(i);
            if (trace_matches_edge(thr_info, edge))
                return true;
        }
        return false;
    }
};
