#pragma once
#include <memory_resource>

#include <Windows.h>

class heap_allocating_resource : public std::pmr::memory_resource
{
public:
    heap_allocating_resource() : heap_handle(HeapCreate(HEAP_NO_SERIALIZE, DEFAULT_SIZE, 0))
    {
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t /* alignment */) override
    {
        return HeapAlloc(heap_handle, 0, bytes);
    }

    void do_deallocate(void* ptr, std::size_t bytes, std::size_t /* alignment */) override
    {
        HeapFree(heap_handle, 0, ptr);
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        if (const auto* heap_mem_res_other = dynamic_cast<const heap_allocating_resource*>(&other))
            return heap_handle == heap_mem_res_other->heap_handle;
        return false;
    }

    HANDLE heap_handle;
    static constexpr std::size_t DEFAULT_SIZE = 10 * 1024 * 1024;
};
