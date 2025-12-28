#ifndef __CONDVAR_HPP__
#define __CONDVAR_HPP__

#include <3ds.h>
#include <Util/Mutex.hpp>

class ConditionVariable {
public:
    ConditionVariable(const ConditionVariable&)            = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

    ConditionVariable();
    ~ConditionVariable();

    CondVar* native_handle();
    Mutex& mutex();

    void broadcast();
    void signal();

    void wait();
    // zero on success, non-zero on failure
    int wait(s64 timeoutNS);

private:
    Mutex m_mutex;
    CondVar m_condVar;
};

#endif