#pragma once

#include <winerror.h>
#include <comdef.h>

#include <stdexcept>
#include <string>
#include <format>
#include <array>
#include <algorithm>

#ifdef assert
  #undef assert
#endif

struct profiler_error : std::runtime_error
{
    static constexpr auto H_ERROR_INSUFFICIENT_BUFFER = static_cast<HRESULT>(0x8007007a);
    static constexpr auto H_ERROR_INVALID_ARGUMENT = static_cast<HRESULT>(0x80070057); // E_INVALIDARG
    static constexpr auto H_ERROR_RECORD_NOT_FOUND = static_cast<HRESULT>(0x80131130); // E_INVALIDARG

    profiler_error(const wchar_t* file, int line, HRESULT res)
        : profiler_error(file + std::wstring(L"#") + std::to_wstring(line) + L": COR function failed with '" + hresult_readable(res) + L"' (" + std::format(L"0x{:x}", static_cast<unsigned long>(res)) + L")")
    {
    }

    profiler_error(std::wstring&& error)
        : runtime_error("<unused, call profiler_error::wwhat()>"), message(std::move(error))
    {
    }

    [[nodiscard]] const std::wstring& wwhat() const
    {
        return message;
    }

    void add_msg(const std::wstring& error)
    {
        message += L"\n  ";
        message += error;
    }

private:
    std::wstring message;

    static std::wstring hresult_readable(HRESULT res)
    {
        _com_error err(res);
        return err.ErrorMessage();
    }
};

namespace detail
{
    template<class T, class... Ts>
    concept all_same = sizeof...(Ts) > 0 && std::conjunction_v
    <
        std::is_same<T, Ts>...
    >;

    struct handle_profiler_error  // NOLINT(cppcoreguidelines-special-member-functions)
    {
        ~handle_profiler_error() = delete;

        static void sanitize(const wchar_t* file, int line, HRESULT value)
        {
            assert(file, line, value, S_OK);
        }

        template<typename... Ts> requires all_same<HRESULT, Ts...>
        static void assert(const wchar_t* file, int line, HRESULT value, Ts... ts)
        {
            std::array<HRESULT, sizeof...(Ts)> arr = { ts... };
            if (std::ranges::none_of(arr, [value](auto val){ return value == val; }))
                throw profiler_error(file, line, value);
        }
    };
}

#define WIDEN_CHAR2(x) L ## x
#define WIDEN_CHAR(x) WIDEN_CHAR2(x)
#define COR_SANITIZE(value) detail::handle_profiler_error::sanitize(WIDEN_CHAR(__FILE__), __LINE__, value)
#define COR_ASSERT(value, ...) detail::handle_profiler_error::assert(WIDEN_CHAR(__FILE__), __LINE__, value, __VA_ARGS__)

#ifdef _DEBUG
#define COR_CATCH_HANDLER_VALUE(ret) catch (const profiler_error& err) { log<logging_level::WARN>(err.wwhat()); return ret; }
#else
#define COR_CATCH_HANDLER_VALUE(ret) catch (const profiler_error&) { return ret; }
#endif
#define COR_CATCH_HANDLER_DEFAULT COR_CATCH_HANDLER_VALUE(E_FAIL)
