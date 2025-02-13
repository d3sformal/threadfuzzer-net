#pragma once
#include "net_types.hpp"

#include <vector>
#include <memory_resource>

class stack_item
{
    const function_spec* function;
    std::size_t native_offset;
    std::size_t il_offset;

public:
    stack_item(const function_spec* function, std::size_t native_offset, std::size_t il_offset)
        : function(function), native_offset(native_offset), il_offset(il_offset)
    {
    }

    [[nodiscard]] const function_spec* get_function() const
    {
        return function;
    }

    [[nodiscard]] std::size_t get_native_offset() const
    {
        return native_offset;
    }

    [[nodiscard]] std::size_t get_il_offset() const
    {
        return il_offset;
    }
};

class stack_info
{
    std::pmr::memory_resource* mem_resource;
    std::pmr::vector<stack_item> stack;
    std::size_t max_stack_size;

public:
    stack_info(std::pmr::memory_resource* mem_resource, std::size_t max_stack_size)
        : mem_resource(mem_resource), stack(mem_resource), max_stack_size(max_stack_size)
    {
        if (max_stack_size)
            stack.reserve(max_stack_size);
    }

    [[nodiscard]] std::pmr::memory_resource* get_memory_resource() const
    {
        return mem_resource;
    }

    bool add_stack_item(const function_spec* function, std::size_t native_offset, std::size_t il_offset)
    {
        if (!max_stack_size || stack.size() < max_stack_size)
        {
            stack.emplace_back(function, native_offset, il_offset);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::size_t size() const
    {
        return stack.size();
    }

    [[nodiscard]] const stack_item& back() const
    {
        return stack.at(0);
    }
};
