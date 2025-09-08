#pragma once

#include "../cor_profiler.hpp"
#include "../thread_preemption_bound.hpp"
#include <memory_resource>

class driver_base
{
    const cor_profiler& profiler;
    std::pmr::memory_resource* mem_resource;

protected:
    const thread_preemption_bound& thread_preemption_bound;

public:
    driver_base(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource, const ::thread_preemption_bound& thread_preemption_bound)
        : profiler(profiler), mem_resource(mem_resource), thread_preemption_bound(thread_preemption_bound)
    {
    }

    [[nodiscard]] const cor_profiler& get_profiler() const
    {
        return profiler;
    }

    driver_base(const driver_base&) = default;
    driver_base(driver_base&&) noexcept = default;

    driver_base& operator=(const driver_base&) = delete;
    driver_base& operator=(driver_base&&) noexcept = delete;

    virtual ~driver_base() = default;

protected:
    [[nodiscard]] std::pmr::memory_resource* get_memory_resource() const
    {
        return mem_resource;
    }
};