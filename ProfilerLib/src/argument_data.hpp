// ReSharper disable CppClangTidyPerformanceNoIntToPtr
// ReSharper disable CppExplicitSpecializationInNonNamespaceScope
#pragma once
#include "cor_error_handling.hpp"

#include <cor.h>

#include <numeric>
#include <string>
#include <vector>

using net_reference = void*;

class argument_data
{
    UINT_PTR start_address;
    std::size_t length;

    static inline std::size_t length_offset;
    static inline std::size_t buffer_offset;

public:
    static void set_string_layout(std::size_t len_offset, std::size_t buf_offset)
    {
        length_offset = len_offset;
        buffer_offset = buf_offset;
    }

    argument_data(UINT_PTR start_address, std::size_t length) : start_address(start_address), length(length)
    {
    }

    template<typename T>
    [[nodiscard]] T as() const;

    template<typename T> requires std::disjunction_v<std::is_integral<T>, std::is_floating_point<T>, std::is_same<T, std::byte>>
    [[nodiscard]] T as() const
    {
        if (sizeof(T) != length)
            throw profiler_error(L"Invalid sizes for T, expected " + std::to_wstring(sizeof(T)) + L" was " + std::to_wstring(length));
        return *reinterpret_cast<const T*>(start_address);
    }

    template<>
    [[nodiscard]] std::wstring as() const
    {
        UINT_PTR objectId = *reinterpret_cast<UINT_PTR*>(start_address);
        if (objectId == 0)
            return L"<null>";

        auto str_length = *reinterpret_cast<DWORD*>(objectId + length_offset);
        auto str_begin = reinterpret_cast<wchar_t*>(objectId + buffer_offset);
        return { str_begin, str_length };
    }

    template<>
    [[nodiscard]] net_reference as() const
    {
        return reinterpret_cast<void*>(start_address);
    }

    template<>
    [[nodiscard]] std::nullptr_t as() const
    {
        throw profiler_error(L"Invalid type");
    }
};

namespace detail
{
    static auto* type_names = new std::vector
    {
        L"System.SByte", L"System.Byte", L"System.Int16", L"System.UInt16", L"System.Int32", L"System.UInt32", L"System.Int64", L"System.UInt64",
        L"System.Single", L"System.Double", L"System.Char", L"System.Boolean",
        L"System.Void", L"System.Object", L"System.String",
        L"System.Int", L"System.UInt",
    };

    using types_t = std::tuple
        <
        std::int8_t, std::byte, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
        std::float_t, std::double_t, wchar_t, bool,
        std::nullptr_t, net_reference, std::wstring,
#ifdef _M_IX86 // x86
        std::int32_t, std::uint32_t
#else
        std::int64_t, std::uint64_t
#endif
        >;

    template<std::size_t I, typename F, typename... Ts>
    static bool dispatch(F&& f, const argument_data& arg_data, const std::wstring& name, const std::vector<const wchar_t*>& names, std::tuple<Ts...>)
    {
        if constexpr (I < sizeof...(Ts))
        {
            if (names[I] == name)
            {
                f(arg_data.as<std::tuple_element_t<I, std::tuple<Ts...>>>());
                return true;
            }
            return dispatch<I + 1, F, Ts...>(std::forward<F>(f), arg_data, name, names, std::tuple<Ts...>{});
        }
        return false;
    }
}

namespace args
{
    template<typename F, typename... Ts>
    static bool dispatch(F&& f, const argument_data& arg_data, const std::wstring& name)
    {
        return detail::dispatch<0, F, Ts...>(std::forward<F>(f), arg_data, name, *detail::type_names, detail::types_t {});
    }
}
