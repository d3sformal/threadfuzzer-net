#include <utils/binary_fstream.hpp>
#include <utils/tree_graph.hpp>
#include <thread_interleaving_control/trace.hpp>

#include <iostream>
#include <iomanip>
#include <queue>
#include <regex>

#undef max

template<typename T>
T read_value(binary_fstream& str)
{
    T t;
    str >> t;
    return t;
}

static constexpr std::size_t BLOCKS_SIZE = 256;

template<typename T>
class nice_hex_manip
{
    T value;
public:
    explicit nice_hex_manip(T&& t)
        : value(std::forward<T>(t))
    {
    }

    friend std::wostream& operator<<(std::wostream& str, const nice_hex_manip& obj)
    {
        std::wios state(nullptr);
        state.copyfmt(str);

        str << L"0x" << std::hex << std::setw(8) << std::setfill(L'0') << obj.value;

        str.copyfmt(state);
        return str;
    }
};

template<typename T>
nice_hex_manip<T> nice_hex(T&& t)
{
    return nice_hex_manip<T>(std::forward<T>(t));
}

void process_vertex(const tree_graph<int, past_trace_item>::graph_vertex& vertex, const std::wstring& edge_desc = L"", int depth = 1)
{
    auto start_line = [](int depth) -> std::wostream& { return std::wcout << std::wstring(depth * 2, L' '); };
    auto make_edge_desc = [](const std::wstring& desc)
    {
        static std::wregex regex(L".*\\.([^(]+)\\(.*");
        std::wsmatch match;
        if (!std::regex_search(desc, match, regex))
        {
            std::wcerr << "No match for " << desc << std::endl;
            throw std::exception("Error");
        }
        std::wstring updated_desc = match[1].str();
        std::ranges::replace(updated_desc, L'_', L' ');
        return L", EL=" + updated_desc;
    };

    start_line(depth) << L"[" << vertex.value << edge_desc;

    if (vertex.edges_size() == 0)
        std::wcout << L"]";
    else
    {
        std::wcout << std::endl;
        for (std::size_t i = 0; i < vertex.edges_size(); ++i)
            process_vertex(vertex.next_vertex(i), make_edge_desc(vertex.get_edge(i).function_id), depth + 1);
        start_line(depth) << L"]";
    }
    std::wcout << std::endl;
}

void compute_vertex_ids(std::unordered_map<const tree_graph<int, past_trace_item>::graph_vertex*, int>& mapping, const tree_graph<int, past_trace_item>::graph_vertex& vertex)
{
    static int id = 0;

    mapping[&vertex] = id++;
    for (std::size_t i = 0; i < vertex.edges_size(); ++i)
        compute_vertex_ids(mapping, vertex.next_vertex(i));
}

void process_graph(binary_fstream&& stream)
{
    trace_file trace_file(std::move(stream));
    tree_graph<int, past_trace_item> call_graph;
    trace_file.populate_call_graph(call_graph);

    process_vertex(call_graph.root());
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc <= 1)
    {
        std::wcerr << L"Usage: " << argv[0] << L" trace_data_file" << std::endl;
        return 1;
    }

    binary_fstream stream(argv[1], binary_fstream::input);
    if (!stream)
    {
        std::wcout << L"Non existing file" << std::endl;
        return 1;
    }

    if (argc > 2 && argv[2] == std::wstring(L"--graph"))
    {
        process_graph(std::move(stream));
        return 0;
    }

    bool no_trace_requested = argc <= 2;
    std::vector<int> requested_traces;
    for (int i = 2; i < argc; ++i)
        requested_traces.push_back(std::stoi(argv[i]));

    std::wcout << L"Is empty: " << std::boolalpha << stream.is_empty_file() << std::endl;
    stream.seek(std::ios::beg);

    auto count = read_value<std::size_t>(stream);
    std::size_t total_blocks = (count / BLOCKS_SIZE) + 1;
    std::wcout << L"Traces count: " << count << L" in " << total_blocks << L" blocks" << std::endl;

    std::vector<std::vector<past_trace_item>> traces(count);

    for (std::size_t cur_block = 0; cur_block < total_blocks; ++cur_block)
    {
        std::wcout << L"Block[" << cur_block << L"]" << std::endl;

        for (std::size_t i = 0; i < BLOCKS_SIZE; ++i)
        {
            auto cur_size = read_value<std::streamoff>(stream);
            if (cur_size != 0)
                std::wcout << L"  Trace[" << BLOCKS_SIZE * cur_block + i << L"] offset: " << nice_hex(cur_size) << std::endl;
        }

        std::wcout << L"  Next block offset: " << nice_hex(read_value<std::streamoff>(stream)) << std::endl;

        for (std::size_t i = 0; i < BLOCKS_SIZE; ++i)
        {
            auto cur_pos = stream.tell();
            auto& cur_trace = traces[cur_block * BLOCKS_SIZE + i];
            stream >> cur_trace;
            if (!stream)
            {
                std::wcout << L"  Trace " << BLOCKS_SIZE * cur_block + i << L" corrupted" << std::endl;
                return 1;
            }

            if (no_trace_requested || std::ranges::find(requested_traces, i) != requested_traces.end())
            {
                std::wcout << L"  Trace " << BLOCKS_SIZE * cur_block + i << L" details: [" << cur_trace.size() << L"] at " << nice_hex(cur_pos) << std::endl;

                for (auto [counted_id, function_id, options_size] : cur_trace)
                    std::wcout << L"    " << std::setw(3) << counted_id << L" " << function_id << L" [" << options_size << L"]" << std::endl;

                std::wcout << std::endl << std::endl;
            }
            if (cur_block * BLOCKS_SIZE + i >= count - 1)
                break;
        }
    }
    for (std::size_t i = 0; i < count; ++i)
    for (std::size_t j = i + 1; j < count; ++j)
    {
        if (no_trace_requested || std::ranges::find(requested_traces, i) != requested_traces.end() || std::ranges::find(requested_traces, j) != requested_traces.end())
            if (std::ranges::find(requested_traces, i) != requested_traces.end() && traces[i] == traces[j])
                std::wcout << L"  Trace " << i << L" and " << j << L" is the same" << std::endl;
    }
}
