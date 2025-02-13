#pragma once

#include "../net_types.hpp"
#include "../thread_id.hpp"
#include "../utils/binary_fstream.hpp"
#include "../utils/tree_graph.hpp"
#include "thread_info.hpp"

#include <vector>
#include <memory_resource>

#undef max

struct trace_item
{
    thread_id thr_id;
    const function_spec* function;
    std::size_t options_size;
};

struct past_trace_item
{
    std::size_t counted_id;
    std::wstring function_id;
    std::size_t options_size;

    friend binary_fstream& operator>>(binary_fstream& str, past_trace_item& item)
    {
        str >> item.counted_id
            && str >> item.function_id
            && str >> item.options_size;
        return str;
    }

    friend binary_fstream& operator<<(binary_fstream& str, const past_trace_item& item)
    {
        str << item.counted_id;
        str << item.function_id;
        str << item.options_size;
        return str;
    }

    friend bool operator==(const past_trace_item& lhs, const past_trace_item& rhs)
    {
        return lhs.counted_id == rhs.counted_id && lhs.function_id == rhs.function_id;
    }
};

class trace
{
    std::pmr::vector<std::pmr::vector<trace_item>> trace_log;
    std::pmr::memory_resource* mem_resource;

public:
    explicit trace(std::pmr::memory_resource* mem_resource)
        : trace_log(mem_resource), mem_resource(mem_resource)
    {
        trace_log.reserve(1024);
    }

    void add(const thread_info* thr_info, std::size_t total_options_size)
    {
        trace_log.emplace_back();
        add_internal(thr_info, total_options_size);
    }

    void add(const std::pmr::vector<thread_info*>& thr_infos, std::size_t total_options_size)
    {
        trace_log.emplace_back();
        for (const thread_info* thr_info : thr_infos)
            add_internal(thr_info, total_options_size);

        if (trace_log.back().empty())
            trace_log.pop_back();
    }

    template<typename F>
    void for_each(F f) const
    {
        for (const auto& vec : trace_log)
            for (const trace_item& item : vec)
                f(item);
    }

    [[nodiscard]] std::size_t size() const
    {
        return trace_log.size();
    }

private:
    void add_internal(const thread_info* thr_info, std::size_t total_options_size)
    {
        if (thr_info->call_stack)
            trace_log.back().emplace_back(thr_info->get_thread_id(), thr_info->call_stack->back(), total_options_size);
    }
};

class trace_file
{
    binary_fstream data_stream;
    static constexpr std::size_t BLOCKS_SIZE = 256;

public:
    explicit trace_file(const std::wstring& path)
        : data_stream(path, binary_fstream::append)
    {
        if (data_stream.is_empty_file())
        {
            // total traces_count
            data_stream << static_cast<std::size_t>(0);
        }
    }

    trace_file(const std::wstring& path, binary_fstream::input_t)
        : data_stream(path, binary_fstream::input)
    {
    }

    trace_file(binary_fstream&& stream)
        : data_stream(std::move(stream))
    {
    }

    std::size_t traces_size()
    {
        data_stream.seek(std::ios::beg);

        std::size_t size;
        data_stream >> size;
        return size;
    }

    template<typename Alloc>
    void get_trace(std::size_t index, std::vector<past_trace_item, Alloc>& past_traces)
    {
        seek_to_trace_offset(index);
        std::streamoff trace_pos;
        data_stream >> trace_pos;

        data_stream.seek(trace_pos);

        data_stream >> past_traces;
    }

    void append_trace(const trace& trace)
    {
        std::size_t traces_count = traces_size();

        if (traces_count % BLOCKS_SIZE == 0)
        {
            data_stream.seek(std::ios::end);
            std::streamoff new_block_offset = data_stream.tell();

            // new block of traces offsets
            for (std::size_t i = 0; i < BLOCKS_SIZE; ++i)
                data_stream << static_cast<std::streamoff>(0);

            // offset of following block
            data_stream << static_cast<std::streamoff>(0);

            if (traces_count)
            {
                data_stream.seek(get_block_offset(traces_count / BLOCKS_SIZE - 1));
                data_stream.seek(std::ios::cur, trace_offset_in_block(BLOCKS_SIZE));
                data_stream << new_block_offset;
            }
        }

        data_stream.seek(std::ios::end);
        std::streamoff cur_trace_pos = data_stream.tell();

        data_stream << trace.size();
        trace.for_each([this](const trace_item& item)
        {
            data_stream << item.thr_id.counted_id << item.function->get_pretty_info() << item.options_size;
        });

        seek_to_trace_offset(traces_count);
        data_stream << cur_trace_pos;

        data_stream.seek(std::ios::beg);
        data_stream << traces_count + 1;
    }

    void populate_call_graph(pmr::tree_graph<int, past_trace_item>& call_graph, std::pmr::memory_resource* mem_res)
    {
        populate_call_graph_impl(call_graph, std::pmr::polymorphic_allocator<past_trace_item>(mem_res));
    }

    void populate_call_graph(tree_graph<int, past_trace_item>& call_graph)
    {
        populate_call_graph_impl(call_graph);
    }

    explicit operator bool() const
    {
        return static_cast<bool>(data_stream);
    }

    bool operator!() const
    {
        return !data_stream;
    }

private:
    template<typename Alloc>
    void populate_call_graph_impl(tree_graph<int, past_trace_item, std::equal_to<>, Alloc>& call_graph, Alloc alloc = Alloc{})
    {
        for (std::size_t i = 0; i < traces_size(); ++i)
        {
            std::vector<past_trace_item, Alloc> trace(alloc);
            get_trace(i, trace);

            // Add traces as edges to graph
            auto* vertex = &call_graph.root();
            for (auto& trace_item : trace)
                vertex = &vertex->add_edge(trace_item, 0);

            // backtrack to root and update the value of vertices
            while ((vertex = vertex->previous_vertex()) != nullptr)
            {
                int current_value = 0;
                for (std::size_t j = 0; j < vertex->edges_size(); ++j)
                    current_value += vertex->next_vertex(j).value;

                // Any edge, options_size might differ if in some run the driver found out weird path
                // Choosing maximum would lead to too many (hard to find) paths
                // Choosing minimum would miss some executions
                current_value += std::max(0, static_cast<int>(vertex->get_edge(0).options_size - vertex->edges_size()));

                vertex->value = current_value;
            }
        }
    }

    void seek_to_trace_offset(std::size_t trace_index)
    {
        data_stream.seek(get_block_offset(trace_index / BLOCKS_SIZE));
        data_stream.seek(std::ios::cur, trace_offset_in_block(trace_index % BLOCKS_SIZE));
    }

    static std::ios::pos_type trace_offset_in_block(std::size_t trace_index)
    {
        return static_cast<std::ios::pos_type>(trace_index) * sizeof(std::streamoff);
    }

    std::ios::pos_type get_block_offset(std::size_t block_index)
    {
        data_stream.seek(std::ios::beg);

        std::streamoff offset = sizeof(std::size_t);
        while (block_index-- > 0)
        {
            offset += static_cast<long long>(BLOCKS_SIZE) * sizeof(std::streamoff);
            data_stream.seek(offset);
            data_stream >> offset;
        }

        return offset;
    }
};
