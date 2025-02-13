#pragma once
#include <cstddef>

struct thread_local_storage
{
    std::size_t thr_counted_id;
};
