#pragma once
#include <thread>

#include <corprof.h>

#include <Windows.h>

struct thread_id
{
    ThreadID managed_id;
    DWORD native_id;
    std::thread::id cpp_id;
    std::size_t counted_id;

    thread_id(ThreadID managed_id, DWORD native_id, std::thread::id cpp_id, std::size_t counted_id)
        : managed_id(managed_id), native_id(native_id), cpp_id(cpp_id), counted_id(counted_id)
    {
    }

    friend auto operator<=>(const thread_id& lhs, const thread_id& rhs)
    {
        return lhs.counted_id <=> rhs.counted_id;
    }
};
