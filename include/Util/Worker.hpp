#ifndef __WORKER_HPP__
#define __WORKER_HPP__

#include <3ds.h>
#include <functional>
#include <string>

// was going to name it thread, but that would conflict with internal types
class Worker {
public:
    enum Processor {
        DEFAULT = -2,
        ALL,
        ONE,
        TWO,
        THREE, // three and four are for new 3ds only
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
    bool waitingForExit() const;

private:
    static void onThreadStart(void* data);

private:
    Thread m_thread;
    bool m_waitingForExit;

    bool m_threadStarted;

    s32 m_priority;
    size_t m_stackSize;
    Processor m_processor;

    std::function<void(Worker*)> m_workerFunction;
};

#endif