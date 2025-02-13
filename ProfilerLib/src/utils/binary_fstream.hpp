#pragma once
#include <type_traits>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include <utility>

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

class binary_fstream
{
    std::fstream stream;

public:
    static constexpr struct append_t
    {
    } append{};

    static constexpr struct input_t
    {
    } input{};

    static constexpr struct output_t
    {
    } output{};

    // Constructors
    binary_fstream(const std::wstring& filename, input_t)
        : stream(filename, std::ios::binary | std::ios::in)
    {
    }

    binary_fstream(const std::wstring& filename, output_t)
        : stream(filename, std::ios::binary | std::ios::out)
    {
    }

    binary_fstream(const std::wstring& filename, append_t)
        : stream(filename, std::ios::binary | std::ios::out | std::ios::in | std::ios::ate)
    {
        if (!stream)
            stream.open(filename, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
    }

private:
    struct detail
    {
        static constexpr struct read_t
        {
            static constexpr bool is_read = true;
            static constexpr bool is_write = !is_read;

            template<typename T>
            static void perform_io(std::fstream& str, T* ptr, std::size_t size)
            {
                str.read(ptr, size);
            }

            template<typename T>
            static binary_fstream& perform_op(binary_fstream& str, T& t)
            {
                return str >> t;
            }
        } read{};

        static constexpr struct write_t
        {
            static constexpr bool is_read = false;
            static constexpr bool is_write = !is_read;

            template<typename T>
            static void perform_io(std::fstream& str, const T* ptr, std::size_t size)
            {
                str.write(ptr, size);
            }

            template<typename T>
            static binary_fstream& perform_op(binary_fstream& str, const T& t)
            {
                return str << t;
            }
        } write{};

        template<typename T>
        using ConstOrNotCharPtr = std::conditional_t<std::is_const_v<T>, const char*, char*>;
    };

    template<typename Operation, Arithmetic V>
    binary_fstream& process_arithmetic(V& value)
    {
        Operation::perform_io(stream, reinterpret_cast<detail::ConstOrNotCharPtr<V>>(&value), sizeof(V));
        return *this;
    }

    template<typename Operation, typename String>
    binary_fstream& process_string(String& str)
    {
        std::size_t sz;
        if constexpr (Operation::is_write)
            sz = str.size();

        if (Operation::perform_op(*this, sz))
        {
            if constexpr (Operation::is_read)
                str.resize(sz);
            Operation::perform_io(stream, detail::ConstOrNotCharPtr<String>(str.data()), static_cast<std::streamsize>(str.size()) * sizeof str[0]);
        }
        return *this;
    }

    template<typename Operation, typename Vector>
    binary_fstream& process_vector(Vector& vec)
    {
        std::size_t sz;
        if constexpr (Operation::is_write)
            sz = vec.size();

        if (Operation::perform_op(*this, sz))
        {
            if constexpr (Operation::is_read)
                vec.resize(sz);
            for (auto&& item : vec)
                if (!Operation::perform_op(*this, item))
                    break;
        }
        return *this;
    }

    template<typename Operation, typename Tuple>
    binary_fstream& process_tuple(Tuple& tuple)
    {
        auto for_each_tuple = [this]<std::size_t... Is>(Tuple& tuple, std::index_sequence<Is...>)
        {
            (Operation::perform_op(*this, std::get<Is>(tuple)) && ...);
        };

        for_each_tuple(tuple, std::make_index_sequence<std::tuple_size_v<Tuple>>());
        return *this;
    }

public:
// Input
    template<Arithmetic V>
    binary_fstream& operator>>(V& value)
    {
        return process_arithmetic<detail::read_t>(value);
    }

    template<typename CharT, typename Alloc>
    binary_fstream& operator>>(std::basic_string<CharT, std::char_traits<CharT>, Alloc>& str)
    {
        return process_string<detail::read_t>(str);
    }

    template<typename T, typename Alloc>
    binary_fstream& operator>>(std::vector<T, Alloc>& vec)
    {
        return process_vector<detail::read_t>(vec);
    }

    template<typename T, typename U>
    binary_fstream& operator>>(std::pair<T, U>& pair)
    {
        return process_tuple<detail::read_t>(pair);
    }

    template<typename... Ts>
    binary_fstream& operator>>(std::tuple<Ts...>& tuple)
    {
        return process_tuple<detail::read_t>(tuple);
    }

// Output
    template<Arithmetic V>
    binary_fstream& operator<<(const V& value)
    {
        return process_arithmetic<detail::write_t>(value);
    }

    template<typename CharT, typename Alloc>
    binary_fstream& operator<<(const std::basic_string<CharT, std::char_traits<CharT>, Alloc>& str)
    {
        return process_string<detail::write_t>(str);
    }

    template<typename T, typename Alloc>
    binary_fstream& operator<<(const std::vector<T, Alloc>& vec)
    {
        return process_vector<detail::write_t>(vec);
    }

    template<typename T, typename U>
    binary_fstream& operator<<(const std::pair<T, U>& pair)
    {
        return process_tuple<detail::write_t>(pair);
    }

    template<typename... Ts>
    binary_fstream& operator<<(const std::tuple<Ts...>& tuple)
    {
        return process_tuple<detail::write_t>(tuple);
    }

    template<typename CharT, std::size_t N>
    binary_fstream& operator<<(CharT (&arr)[N])
    {
        if (*this << N)
            stream.write(reinterpret_cast<const char*>(arr), static_cast<std::streamsize>(N) * sizeof arr[0]);
        return *this;
    }

// Miscellaneous
    explicit operator bool() const
    {
        return static_cast<bool>(stream);
    }

    bool operator!() const
    {
        return !stream;
    }

    void seek(std::ios::pos_type pos)
    {
        stream.seekp(pos);
    }

    void seek(std::ios::seekdir dir, std::ios::off_type off = 0)
    {
        stream.seekp(off, dir);
    }

    void seek(std::streamoff pos)
    {
        stream.seekp(pos);
    }

    std::ios::pos_type tell()
    {
        return stream.tellp();
    }

    bool is_empty_file()
    {
        seek(std::ios::end);
        return tell() == 0;
    }
};
