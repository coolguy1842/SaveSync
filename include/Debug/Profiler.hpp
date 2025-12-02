#ifndef __PROFILER_HPP__
#define __PROFILER_HPP__

#include <3ds.h>
#include <Util/Mutex.hpp>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef DEBUG
#define CONCAT_INTERNAL(a, b) a##b
#define CONCAT(a, b)          CONCAT_INTERNAL(a, b)

#define PROFILE_SCOPE(name) ProfilerScope CONCAT(scope, __COUNTER__) = Profiler::start(name);
#else
#define PROFILE_SCOPE(name)
#endif

class ProfilerScope {
public:
    ~ProfilerScope();

private:
    ProfilerScope(bool valid = false, u64 id = 0);
    bool m_valid;
    u64 m_id;

    friend class Profiler;
};

// this class is mostly just a stub in release mode
class Profiler {
public:
    struct AveragedEntry {
        std::string scopeName;
        u64 average;
    };

    // doesn't require stopping, ends when block ends
    [[nodiscard]] static ProfilerScope start(std::string scopeName);

    static void reset();
    // if scope not profiled, returns 0
    static u64 getScopeAverage(std::string scopeName);
    static std::vector<AveragedEntry> getAverages();

private:
    static void stop(u64 id);

    struct RunningEntry {
        std::string scopeName;
        u64 start;
    };

    struct ProfilerEntry {
        u64 offsets = 0;
        u64 count   = 0;
    };

    // key is the thread priority, number is start time
    static std::map<u64, RunningEntry> s_runningEntries;
    static std::map<std::string, ProfilerEntry> s_entries;

    static Mutex s_entriesMutex;
    static Mutex s_runningEntriesMutex;

    friend class ProfilerScope;
};

#endif