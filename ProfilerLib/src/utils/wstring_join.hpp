#pragma once
#include <string>
#include <memory>
#include <memory_resource>

namespace tmt
{
    template<typename Range, typename Alloc = std::allocator<wchar_t>>
    static std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc> wstring_join(const Range& container, const std::wstring& delimiter, Alloc alloc = Alloc{})
    {
        std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc> result(alloc);
        result.reserve(128);
        for (const auto& element : container)
        {
            if constexpr (std::is_same_v<std::wstring, std::decay_t<decltype(element)>>)
                result += element;
            else
                result += std::to_wstring(element);
            result += delimiter;
        }
        if (!container.empty())
            result.resize(result.size() - delimiter.size());

        return result;
    }

    namespace pmr
    {
        template<typename Range>
        static std::pmr::wstring wstring_join(std::pmr::memory_resource* mem_res, const Range& container, const std::wstring& delimiter)
        {
            return ::tmt::wstring_join<Range, std::pmr::polymorphic_allocator<wchar_t>>(container, delimiter, mem_res);
        }
    }
}
