#include <Debug/Profiler.hpp>
#include <atomic>

ProfilerScope::ProfilerScope(bool valid, u64 id)
    : m_valid(valid)
    , m_id(id) {}

ProfilerScope::~ProfilerScope() {
#ifdef DEBUG
    if(!m_valid) {
        return;
    }

    Profiler::stop(m_id);
#endif
}

std::map<u64, Profiler::RunningEntry> Profiler::s_runningEntries   = {};
std::map<std::string, Profiler::ProfilerEntry> Profiler::s_entries = {};

Mutex Profiler::s_entriesMutex        = Mutex();
Mutex Profiler::s_runningEntriesMutex = Mutex();
ProfilerScope Profiler::start(std::string scopeName) {
#ifdef DEBUG
    static std::atomic<u64> nextId = 0;
    u64 id                         = nextId++;

    {
        auto lock = s_entriesMutex.lock();
        if(!s_entries.contains(scopeName)) {
            s_entries[scopeName] = ProfilerEntry{
                .offsets = 0,
                .count   = 0
            };
        }
    }

    auto lock            = s_runningEntriesMutex.lock();
    s_runningEntries[id] = RunningEntry{
        .scopeName = scopeName,
        .start     = svcGetSystemTick()
    };

    return ProfilerScope(true, id);
#else
    (void)scopeName;
    return ProfilerScope();
#endif
}

void Profiler::reset() {
#ifdef DEBUG
    auto lock  = s_entriesMutex.lock();
    auto lock2 = s_runningEntriesMutex.lock();

    s_entries.clear();
    s_runningEntries.clear();
#endif
}

u64 Profiler::getScopeAverage(std::string scopeName) {
#ifdef DEBUG
    auto lock = s_entriesMutex.lock();
    if(!s_entries.contains(scopeName)) {
        return 0;
    }

    const ProfilerEntry& entry = s_entries[scopeName];
    return entry.offsets / entry.count;
#else
    (void)scopeName;
    return 0;
#endif
}

std::vector<Profiler::AveragedEntry> Profiler::getAverages() {
#ifdef DEBUG
    auto lock = s_entriesMutex.lock();

    std::vector<AveragedEntry> averages;
    averages.reserve(s_entries.size());

    for(const auto& entry : s_entries) {
        if(entry.second.count == 0) {
            continue;
        }

        averages.push_back(AveragedEntry{
            .scopeName = entry.first,
            .average   = entry.second.offsets / entry.second.count,
        });
    }

    return averages;
#else
    return {};
#endif
}

void Profiler::stop(u64 id) {
#ifdef DEBUG
    const u64 stop = svcGetSystemTick();

    auto lock = s_runningEntriesMutex.lock();

    auto it = s_runningEntries.find(id);
    if(it == s_runningEntries.end()) {
        return;
    }

    const RunningEntry& entry = it->second;
    auto lock2                = s_entriesMutex.lock();

    ProfilerEntry& pEntry = s_entries[entry.scopeName];
    pEntry.offsets += stop - entry.start;
    pEntry.count++;

    s_runningEntries.erase(it);
#else
    (void)id;
#endif
}