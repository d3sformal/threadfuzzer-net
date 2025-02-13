#pragma once
#include <optional>
#include <atomic>
#include <thread>

template<typename T>
class atomic_value_exchanger
{
    std::optional<T> m_value;
    std::atomic<const T*> spin_lock;

public:
    class raii_release
    {
        atomic_value_exchanger<T>* value_to_delete;

    public:
        raii_release()
            : raii_release(nullptr)
        {
        }

        raii_release(atomic_value_exchanger<T>* value_to_delete)
            : value_to_delete(value_to_delete)
        {
        }

        raii_release(const raii_release&) = delete;
        raii_release& operator=(const raii_release&) = delete;

        raii_release(raii_release&& other) noexcept
            : value_to_delete(other.value_to_delete)
        {
            other.value_to_delete = nullptr;
        }
        raii_release& operator=(raii_release&& other) noexcept
        {
            value_to_delete = other.value_to_delete;
            other.value_to_delete = nullptr;
            return *this;
        }

        ~raii_release()
        {
            if (value_to_delete)
            {
                value_to_delete->m_value.reset();
                value_to_delete->spin_lock.store(nullptr);
            }
        }
    };

    void store(const T& value)
    {
        store_internal(&value);
        m_value = value;
        wait_until_consumed();
    }

    void store(T&& value)
    {
        store_internal(&value);
        m_value = std::move(value);
        wait_until_consumed();
    }

    std::pair<T*, raii_release> load()
    {
        if (!m_value.has_value())
            return { nullptr, {} };

        if (const T* value; (value = spin_lock.load()) != nullptr)
            return { &m_value.value(), this };
        return { nullptr, {} };
    }

private:
    void store_internal(const T* value_ptr)
    {
        const T* expected = nullptr;
        while (!spin_lock.compare_exchange_weak(expected, value_ptr))
        {
            expected = nullptr;
            std::this_thread::yield();
        }
    }

    void wait_until_consumed()
    {
        while (spin_lock.load() != nullptr)
            std::this_thread::yield();
    }
};
