#pragma once

#include "thread_info.hpp"
#include "driver_base.hpp"
#include "trace.hpp"

#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <algorithm>

class fuzzing_driver : public driver_base
{
    std::size_t seed;
    std::mt19937 rng_engine;

public:
    fuzzing_driver(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource, std::size_t seed = std::random_device{}())
        : driver_base(profiler, mem_resource), seed(seed), rng_engine(seed)
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
        std::pmr::vector<thread_info*> result(1, get_memory_resource());
        std::ranges::sample(thread_infos | views::only_frozen, result.data(), 1, rng_engine);

        return result;
    }

    [[nodiscard]] std::size_t current_seed() const
    {
        return seed;
    }

    static bool should_update_data_file()
    {
        return true;
    }
};
