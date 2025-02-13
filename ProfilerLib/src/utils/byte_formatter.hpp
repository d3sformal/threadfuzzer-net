#pragma once

#include <format>
#include <cstddef>

template<typename CharT>
struct std::formatter<std::byte, CharT> : std::formatter<int, CharT>
{
    template<class FormatContext>
    auto format(std::byte b, FormatContext& fc) const
    {
        return std::formatter<int, CharT>::format(static_cast<int>(b), fc);
    }
};
