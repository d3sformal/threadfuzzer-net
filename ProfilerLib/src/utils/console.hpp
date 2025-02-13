#pragma once
#include <string>
#include <iterator>
#include <algorithm>
#include <random>

#include <named_pipe.hpp>
#include "process.hpp"

#include <wchar.h>

class console
{
    std::wstring rnd_pipe_name;
    named_pipe pipe;
    process_t process;

    static std::wstring random_str()
    {
        auto rng = []
        {
            static std::wstring characters_base = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            static std::mt19937 engine(std::random_device{}());
            static std::uniform_int_distribution<> dist(0, static_cast<int>(characters_base.size()) - 1);
            return characters_base[dist(engine)];
        };

        constexpr std::size_t SIZE = 8;
        std::wstring result;
        std::generate_n(std::back_inserter(result), SIZE, rng);

        return result;
    }

    static std::wstring exe_name()
    {
        static const std::wstring ENV_NAME = L"COR_PROFILER_CONSOLE_PROVIDER";
        wchar_t* exe_path = nullptr;
        std::size_t unused;
        COR_SANITIZE(_wdupenv_s(&exe_path, &unused, ENV_NAME.c_str()));
        std::unique_ptr<wchar_t, decltype(free)*> storage { exe_path, free };

        return exe_path;
    }

public:
    console()
        : rnd_pipe_name(random_str())
        , pipe(rnd_pipe_name, 4096, named_pipe::CREATE)
        , process(exe_name(), rnd_pipe_name)
    {
        pipe.connect();
    }

    console(console&&) = default;
    console& operator=(console&&) = default;

    ~console()
    {
        pipe.write(named_pipe::EOT);
    }

    void print(const std::wstring& str)
    {
        pipe.write(named_pipe::WRITE);
        pipe.write(str);
    }

    void print(const std::pmr::wstring& str)
    {
        pipe.write(named_pipe::WRITE);
        pipe.write(str.c_str(), str.size());
    }

    std::wstring getline()
    {
        pipe.write(named_pipe::READ);
        return pipe.read();
    }
};
