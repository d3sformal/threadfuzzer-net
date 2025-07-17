#pragma once
#include <numeric>

enum class thread_preemption_bound_strategy : std::uint8_t
{
    CONTINUE,
    EXIT,
};

struct thread_preemption_bound
{
    std::size_t max;
    std::atomic<std::size_t> current;
    thread_preemption_bound_strategy strategy;

    explicit thread_preemption_bound(std::size_t max) : max(max), current(0), strategy(thread_preemption_bound_strategy::CONTINUE)
    {
    }

    thread_preemption_bound& operator=(const thread_preemption_bound&) = delete;
    thread_preemption_bound(const thread_preemption_bound&) = delete;

    thread_preemption_bound& operator=(thread_preemption_bound&& tpb) noexcept
    {
        max = tpb.max;
        current = tpb.current.load();
        strategy = tpb.strategy;
        return *this;
    }

    thread_preemption_bound(thread_preemption_bound&& tpb) noexcept : max(tpb.max), current(tpb.current.load()), strategy(tpb.strategy)
    {
    }

    ~thread_preemption_bound() = default;
};
