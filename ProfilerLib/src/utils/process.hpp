#pragma once
#include <string>
#include <stdexcept>

#include <Windows.h>

class process_t
{
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;

public:
    process_t(const std::wstring& exe_name, const std::wstring& args)
        : startup_info(), process_info()
    {
        std::memset(&startup_info, 0, sizeof(startup_info));
        std::memset(&process_info, 0, sizeof(process_info));
        startup_info.cb = sizeof(startup_info);

        auto command_line = exe_name + L" " + args;
        if (!CreateProcess(nullptr, command_line.data(), nullptr, nullptr, false, CREATE_NEW_CONSOLE, nullptr, nullptr, &startup_info, &process_info))
            throw std::runtime_error("CreateProcess failed with code" + std::to_string(GetLastError()));
    }

    process_t(const process_t&) = delete;
    process_t& operator=(const process_t&) = delete;

    process_t(process_t&& other) noexcept  // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        *this = std::move(other);
    }

    process_t& operator=(process_t&& other) noexcept
    {
        startup_info = other.startup_info;
        process_info = other.process_info;
        other.process_info.hProcess = nullptr;
        other.process_info.hThread = nullptr;

        return *this;
    }

    ~process_t()
    {
        if (process_info.hProcess)
            CloseHandle(process_info.hProcess);
        if (process_info.hThread)
            CloseHandle(process_info.hThread);
    }

    void wait() const
    {
        WaitForSingleObject(process_info.hProcess, INFINITE);
    }
};
