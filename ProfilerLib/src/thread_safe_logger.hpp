#pragma once
#include <mutex>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <chrono>

#undef ERROR

struct logging_level
{
    static inline constexpr int ERROR = 1;
    static inline constexpr int WARN = 2;
    static inline constexpr int INFO = 3;
    static inline constexpr int VERBOSE = 4;
    static inline constexpr int DEBUG = 5;

    template<int Level>
    static constexpr const wchar_t* text()
    {
        if constexpr (Level == ERROR)
            return L"Error";
        if constexpr (Level == WARN)
            return L"Warn";
        if constexpr (Level == INFO)
            return L"Info";
        if constexpr (Level == VERBOSE)
            return L"Verbose";
        if constexpr (Level == DEBUG)
            return L"Debug";
        return L"";
    }
};

class logging_target
{
    std::unique_ptr<std::wofstream> storage;
    std::wostream* target;
    int logging_level;
    std::chrono::steady_clock::time_point profiler_start;

    std::unique_ptr<std::mutex> mtx;

    logging_target(std::wofstream* storage, std::wostream* target, int level)
        : storage(storage), target(target), logging_level(level), profiler_start(std::chrono::steady_clock::now()), mtx(level > 0 ? new std::mutex : nullptr)
    {
    }

public:
    logging_target(int level, std::wostream& stream)
        : logging_target(nullptr, &stream, level)
    {
    }

    logging_target(int level, const std::wstring& file_name)
        : logging_target(!file_name.empty() ? new std::wofstream(file_name) : nullptr, nullptr, level)
    {
        target = storage.get();
    }

    logging_target()
        :logging_target(nullptr, nullptr, 0)
    {
    }

    logging_target(const logging_target&) = delete;
    logging_target& operator=(const logging_target&) = delete;

    logging_target(logging_target&&) = default;
    logging_target& operator=(logging_target&&) = default;
    ~logging_target() = default;

    template<int Level>
    [[nodiscard]] bool is_logging() const
    {
        return logging_level >= Level && target != nullptr;
    }

    template<int Level, typename... Ts>
    void print_line(Ts&&... ts) const
    {
        print_line<Level>(std::pmr::get_default_resource(), std::forward<Ts>(ts)...);
    }

    template<int Level, typename... Ts>
    void print_line(std::pmr::memory_resource* mem_res, Ts&&... ts) const
    {
        if (!is_logging<Level>())
            return;

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - profiler_start).count();

        std::pmr::wstring prefix(mem_res);
        std::format_to(std::back_inserter(prefix), L"[{:10}] [{:5}] [{}] ", duration, GetCurrentThreadId(), logging_level::text<Level>());

        std::lock_guard lock(*mtx);
        *target << std::boolalpha;
        *target << prefix;
        (*target << ... << fix_utf16(ts));
        *target << std::endl;
    }

private:
    template<typename T, typename = std::enable_if_t<!std::is_same_v<
        std::remove_reference_t<T>,
        std::wstring
        >>>
    static T&& fix_utf16(T&& t)
    {
        return std::forward<T>(t);
    }

    static std::wstring fix_utf16(std::wstring&& str)
    {
        replace_all_inline(str, std::wstring(1, L'\0'), L"<NUL>");
        replace_all_inline(str, std::wstring(1, L'\ufffd'), L"<U+FFFD>");

        return std::move(str);
    }

    static std::wstring fix_utf16(const std::wstring& str)
    {
        std::wstring newstr = str;
        return fix_utf16(std::move(newstr));
    }

    static std::wstring fix_utf16(wchar_t c)
    {
        if (c > 10 && c < 128)
            return std::wstring(1, c);
        return L"C " + std::to_wstring(c);
    }

    static void replace_all_inline(std::wstring& input, const std::wstring& what, const std::wstring& to)
    {
        std::size_t start_pos = 0;
        while ((start_pos = input.find(what, start_pos)) != std::string::npos)
        {
            input.replace(start_pos, what.length(), to);
            start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        }
    }
};

inline std::wostream& operator<<(std::wostream& stream, std::byte byte)
{
    stream << static_cast<int>(byte);
    return stream;
}
