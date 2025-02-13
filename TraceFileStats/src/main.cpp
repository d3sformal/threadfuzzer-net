#include <iostream>
#include <unordered_map>
#include <thread_interleaving_control/trace.hpp>
#include <utils/wstring_join.hpp>
#include <typeinfo>

using single_trace = std::vector<past_trace_item>;

namespace detail
{
    template<typename T, typename F>
    std::size_t hash_compute(std::size_t seed, const T& value, F hash_t)
    {
        return hash_t(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<typename T, typename... Rest>
    void hash_combine(std::size_t& seed, const T& value, const Rest&... rest)
    {
        seed ^= hash_compute(seed, value, std::hash<T>{});
        (hash_combine(seed, rest), ...);
    }
}

template<typename T, typename... Rest>
std::size_t hash_combine(const T& v, const Rest&... rest)
{
    std::size_t seed = 0;
    detail::hash_combine(seed, v, rest...);
    return seed;
}

template<typename T, typename F>
std::size_t hash_vector(const std::vector<T>& vector, F hash_t)
{
    std::size_t seed = 0;
    for (const T& value : vector)
        seed ^= detail::hash_compute(seed, value, hash_t);
    return seed;
}

template<typename Key, typename Value, typename Hash, typename Compare = std::equal_to<Key>>
std::unordered_map<Key, Value, Hash, Compare> make_unordered_map(Hash hash, Compare compare = Compare{})
{
    return std::unordered_map<Key, Value, Hash, Compare>(1, hash, compare);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc <= 1)
    {
        std::wcerr << L"Usage: " << argv[0] << L" trace_data_file [debug]" << std::endl;
        return 1;
    }

    trace_file trace_log(argv[1], binary_fstream::input);

    if (!trace_log)
    {
        std::wcerr << L"'" << argv[1] << L"' is not a valid file" << std::endl;
        return 1;
    }

    bool debug = argc > 2 && argv[2] == std::wstring(L"debug");

    std::vector<std::vector<past_trace_item>> traces(trace_log.traces_size());
    for (std::size_t i = 0; i < traces.size(); ++i)
        trace_log.get_trace(i, traces[i]);

    auto counted_id_compare = [](const past_trace_item& st1, const past_trace_item& st2)
    {
        return st1.counted_id == st2.counted_id;
    };
    
    for (int i = 1; single_trace trace : traces) // explicit copy
    {
        std::wcout << L"Trace " << i << L": " << trace.size();

        auto erase_begin = std::ranges::unique(trace, counted_id_compare).begin();
        trace.erase(erase_begin, trace.end());

        std::wcout << L" / " << trace.size() << std::endl;
        ++i;
    }
    std::wcout << std::endl;

    auto hash_trace_exact = [](const past_trace_item& item) { return hash_combine(item.counted_id, item.function_id); };
    auto make_vector_hasher = [](auto& hash_func) { return [hash_func](const single_trace& vec) { return hash_vector(vec, hash_func); }; };

    auto compare_exact = [](const single_trace& t1, const single_trace& t2) { return std::ranges::equal(t1, t2); };
    
    auto same_traces = make_unordered_map<single_trace, std::vector<std::size_t>>(make_vector_hasher(hash_trace_exact), compare_exact);

    for (std::size_t i = 0; i < traces.size(); ++i)
        same_traces[traces[i]].push_back(i);

    std::wcout << L"Unique traces" << L": " << same_traces.size() << std::endl;
    if (debug)
    {
        std::wcout << L"{" << std::endl;
        for (auto&& ids : same_traces | std::views::values)
            if (ids.size() > 1)
                std::wcout << L"    [" << tmt::wstring_join(ids, L", ") << L"]" << std::endl;
        std::wcout << L"}" << std::endl;
    }
}
