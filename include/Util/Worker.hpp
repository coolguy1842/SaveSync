#ifndef __WORKER_HPP__
#define __WORKER_HPP__

#include <3ds.h>
#include <Util/Mutex.hpp>
#include <atomic>
#include <functional>
#include <string>

// was going to name it thread, but that would conflict with internal types
class Worker {
public:
    enum Processor {
        DEFAULT = -2,
        ALL,
        APPCORE,
        SYSCORE, // from libctru: If APT_SetAppCpuTimeLimit is used, it is possible to create a single thread on this core.
        THREE,   // three and four are for new 3ds only
        FOUR
    };

    Worker(const Worker&) = delete;

    // higher priority is better, defaults to one higher than the current thread
    Worker(std::function<void(Worker*)> workerFunction = nullptr, int priorityOffset = 1, size_t stackSize = 0x1000, Processor processor = DEFAULT);
    ~Worker();

    void setWorkerFunc(std::function<void(Worker*)> func = nullptr);

    bool running() const;
    void start();

    void waitForExit();
    // sets waitingForExit, but does not block
    void signalShouldExit();

    bool waitingForExit() const;

private:
    static void onThreadStart(void* data);

private:
    Thread m_thread;
    std::atomic<bool> m_waitingForExit;
    std::atomic<bool> m_joining;

    bool m_threadStarted;

    s32 m_priority;
    size_t m_stackSize;
    Processor m_processor;

    Mutex m_mutex;
    std::function<void(Worker*)> m_workerFunction;
};

#endif