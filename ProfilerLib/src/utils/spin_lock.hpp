#pragma once
#include <atomic>

// https://rigtorp.se/spinlock/

class spin_lock
{
    std::atomic<bool> m_lock;
public:
    void lock() noexcept
    {
        for (;;) {
            // Optimistically assume the lock is free on the first try
            if (!m_lock.exchange(true, std::memory_order_acquire)) {
                return;
            }

            // Wait for lock to be released without generating cache misses
            while (m_lock.load(std::memory_order_relaxed)) {
                // Issue pause/yield instruction to reduce contention between hyper-threads
                pause_instr();
            }
        }
    }

    void unlock() noexcept {
        m_lock.store(false, std::memory_order_release);
    }

private:
    static void pause_instr()
    {
#ifdef _WIN32 
        _mm_pause();
#else
        static_assert("Unsupported platform");
#endif
    }
};
