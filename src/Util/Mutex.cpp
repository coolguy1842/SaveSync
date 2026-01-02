#include <Util/Mutex.hpp>
#include <stdio.h>

ScopedLock::ScopedLock()
    : m_mutex(nullptr)
    , m_locked(false) {}

ScopedLock::ScopedLock(Mutex& mutex, bool deffered)
    : m_mutex(&mutex)
    , m_locked(false) {
    if(deffered || m_mutex == nullptr) {
        return;
    }

    lock();
}

ScopedLock::ScopedLock(Mutex* mutex, bool deffered)
    : m_mutex(mutex)
    , m_locked(false) {
    if(deffered || m_mutex == nullptr) {
        return;
    }

    lock();
}

ScopedLock::~ScopedLock() {
    if(m_locked) {
        m_mutex->unsafe_unlock();
    }
}

void ScopedLock::lock() {
    if(m_mutex == nullptr || m_locked) {
        return;
    }

    m_mutex->unsafe_lock();
    m_locked = true;
}

void ScopedLock::release() {
    if(m_mutex == nullptr || !m_locked) {
        m_locked = false;
        return;
    }

    m_mutex->unsafe_unlock();
    m_locked = false;
}

Mutex::Mutex() { LightLock_Init(&m_lock); }
Mutex::~Mutex() { unsafe_unlock(); }
bool Mutex::locked() const { return m_locked; }
LightLock* Mutex::native_handle() { return &m_lock; }

void Mutex::unsafe_lock() {
    m_locked = true;
    LightLock_Lock(&m_lock);
}

void Mutex::unsafe_unlock() {
    LightLock_Unlock(&m_lock);
    m_locked = false;
}

ScopedLock Mutex::lock() {
    return ScopedLock(this);
}