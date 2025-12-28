#include <Util/CondVar.hpp>

ConditionVariable::ConditionVariable() {
    CondVar_Init(&m_condVar);
}

ConditionVariable::~ConditionVariable() {
    // wake up all threads hanging on condvar
    broadcast();
}

CondVar* ConditionVariable::native_handle() { return &m_condVar; }
Mutex& ConditionVariable::mutex() { return m_mutex; }

void ConditionVariable::broadcast() {
    auto lock = m_mutex.lock();
    CondVar_Broadcast(&m_condVar);
}

void ConditionVariable::signal() {
    auto lock = m_mutex.lock();
    CondVar_Signal(&m_condVar);
}

void ConditionVariable::wait() {
    auto lock = m_mutex.lock();
    CondVar_Wait(&m_condVar, m_mutex.native_handle());
}

int ConditionVariable::wait(s64 timeoutNS) {
    auto lock = m_mutex.lock();
    return CondVar_WaitTimeout(&m_condVar, m_mutex.native_handle(), timeoutNS);
}