#include <Util/Worker.hpp>
#include <algorithm>
#include <stdio.h>

Worker::~Worker() { waitForExit(); }
Worker::Worker(std::function<void(Worker*)> workerFunction, int priorityOffset, size_t stackSize, Processor processor)
    : m_thread(nullptr)
    , m_waitingForExit(false)
    , m_threadStarted(false)
    , m_stackSize(stackSize)
    , m_processor(processor)
    , m_workerFunction(workerFunction) {
    s32 priority;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);

    m_priority = std::clamp(priority - priorityOffset, 0x18L, 0x3FL);
}

void Worker::setWorkerFunc(std::function<void(Worker*)> func) { m_workerFunction = func; }

bool Worker::running() const { return m_threadStarted && threadGetExitCode(m_thread) == 0; }
bool Worker::waitingForExit() const { return m_waitingForExit; }

void Worker::start() {
    if(m_workerFunction == nullptr || running()) {
        return;
    }

    waitForExit();

    m_threadStarted = true;
    m_thread        = threadCreate(&Worker::onThreadStart, this, m_stackSize, m_priority, m_processor, false);
}

void Worker::waitForExit() {
    if(m_thread != nullptr) {
        if(running()) {
            m_waitingForExit = true;

            threadJoin(m_thread, U64_MAX);
        }

        threadFree(m_thread);
    }

    m_thread = nullptr;

    m_threadStarted  = false;
    m_waitingForExit = false;
}

void Worker::onThreadStart(void* data) {
    Worker* worker = reinterpret_cast<Worker*>(data);

    if(worker->m_workerFunction != nullptr) {
        worker->m_workerFunction(worker);
    }

    threadExit(1);
}