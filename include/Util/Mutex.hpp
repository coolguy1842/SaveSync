#ifndef __MUTEX_HPP__
#define __MUTEX_HPP__

#include <3ds.h>
#include <atomic>

class Mutex;
class ScopedLock {
public:
    ScopedLock(const ScopedLock&)            = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

    ScopedLock();
    ScopedLock(Mutex& mutex, bool deferred = false);
    ScopedLock(Mutex* mutex, bool deferred = false);

    ~ScopedLock();

    void lock();
    void release();

private:
    Mutex* m_mutex;
    bool m_locked;

    friend Mutex;
};

class Mutex {
public:
    Mutex(const Mutex&)            = delete;
    Mutex& operator=(const Mutex&) = delete;

    Mutex();
    ~Mutex();

    bool locked() const;
    LightLock* native_handle();

    void unsafe_lock();
    void unsafe_unlock();

    [[nodiscard]] ScopedLock lock();

private:
    LightLock m_lock;
    std::atomic<bool> m_locked;

    friend ScopedLock;
};

#endif