#pragma once

#include <Windows.h>

#include <string>
#include <stdexcept>

class named_pipe
{
    std::wstring pipe_name;
    std::wstring buffer;

    HANDLE pipe_handle;

public:
    inline static const std::wstring EOT = L"EOT";
    inline static const std::wstring WRITE = L"WRITE";
    inline static const std::wstring READ = L"READ";

    enum mode_t
    {
        CREATE,
        CONNECT,
    };

    named_pipe(const std::wstring& pipe_name, std::size_t buf_size, mode_t mode)
        : pipe_name(LR"(\\.\pipe\)" + pipe_name), buffer(buf_size, L'\0')
    {
        if (mode == mode_t::CREATE)
            pipe_handle = CreateNamedPipe(this->pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE, 1, 0, 0, 0, nullptr);
        else if (mode == mode_t::CONNECT)
            pipe_handle = CreateFile(this->pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        else
            throw std::runtime_error("mode_t enum mismatch");

        if (pipe_handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Pipe creation failed with code " + std::to_string(GetLastError()));
    }

    named_pipe(const named_pipe&) = delete;
    named_pipe& operator=(const named_pipe&) = delete;
    
    named_pipe(named_pipe&& other) noexcept
    {
        *this = std::move(other);
    }

    named_pipe& operator=(named_pipe&& other) noexcept
    {
        pipe_name = std::move(other.pipe_name);
        buffer = std::move(other.buffer);
        pipe_handle = other.pipe_handle;
        other.pipe_handle = nullptr;
        return *this;
    }

    ~named_pipe()
    {
        if (pipe_handle)
            CloseHandle(pipe_handle);
    }

    std::wstring read()
    {
        if (!pipe_handle)
            return L"";

    	DWORD bytes_read;
        if (!ReadFile(pipe_handle, buffer.data(), buffer.size() * sizeof(buffer[0]), &bytes_read, nullptr))
            throw std::runtime_error("Pipe read failed with code " + std::to_string(GetLastError()));

        buffer[bytes_read / sizeof(buffer[0])] = L'\0';
        return buffer.substr(0, bytes_read / sizeof(buffer[0]));
    }

    void write(const wchar_t* data, std::size_t length)
    {
        if (!pipe_handle)
            return;

        DWORD bytes_written;
        if (!WriteFile(pipe_handle, data, length * sizeof(data[0]), &bytes_written, nullptr))
            throw std::runtime_error("Pipe write failed with code " + std::to_string(GetLastError()));
    }

    void write(const std::wstring& str)
    {
        write(str.c_str(), str.size());
    }

    void connect()
    {
        if (!pipe_handle)
            return;

    	if (!ConnectNamedPipe(pipe_handle, nullptr))
            throw std::runtime_error("Pipe connect failed with code " + std::to_string(GetLastError()));
    }
};
