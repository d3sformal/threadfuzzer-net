#pragma once
#include <vector>
#include <string>
#include <regex>
#include <memory>
#include <memory_resource>

namespace tmt
{
    template<typename Alloc = std::allocator<std::wstring>>
    static std::vector<std::wstring, Alloc> wstring_split(const std::wstring& str, const std::wregex& regex, Alloc alloc = Alloc{})
    {
        std::wsregex_token_iterator begin{ str.begin(), str.end(), regex, -1 };
        return { begin, {}, alloc };
    }

    namespace pmr
    {
        static std::pmr::vector<std::wstring> wstring_split(std::pmr::memory_resource* mem_res, const std::wstring& str, const std::wregex& regex)
        {
            return ::tmt::wstring_split<std::pmr::polymorphic_allocator<std::wstring>>(str, regex, mem_res);
        }
    }
}
