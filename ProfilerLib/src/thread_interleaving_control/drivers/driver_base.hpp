#pragma once

#include "../cor_profiler.hpp"
#include <memory_resource>

class driver_base
{
    const cor_profiler& profiler;
    std::pmr::memory_resource* mem_resource;

public:
    driver_base(const cor_profiler& profiler, std::pmr::memory_resource* mem_resource)
        : profiler(profiler), mem_resource(mem_resource)
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