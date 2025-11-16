#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
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

std::unordered_map<u64, Profiler::RunningEntry> Profiler::s_runningEntries = {};
std::map<std::string, Profiler::ProfilerEntry> Profiler::s_entries         = {};

std::mutex Profiler::s_entriesMutex        = std::mutex();
std::mutex Profiler::s_runningEntriesMutex = std::mutex();
ProfilerScope Profiler::start(std::string scopeName) {
#ifdef DEBUG
    static std::atomic<u64> nextId = 0;
    u64 id                         = nextId++;

    {
        std::unique_lock lock(s_entriesMutex);
        if(!s_entries.contains(scopeName)) {
            s_entries[scopeName] = ProfilerEntry{
                .offsets = 0,
                .count   = 0
            };
        }
    }

    std::unique_lock lock(s_runningEntriesMutex);
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
    std::unique_lock lock(s_entriesMutex);
    std::unique_lock lock2(s_runningEntriesMutex);

    s_entries.clear();
    s_runningEntries.clear();
#endif
}

u64 Profiler::getScopeAverage(std::string scopeName) {
#ifdef DEBUG
    std::unique_lock lock(s_entriesMutex);
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
    std::unique_lock lock(s_entriesMutex);

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

    std::unique_lock lock(s_runningEntriesMutex);

    auto it = s_runningEntries.find(id);
    if(it == s_runningEntries.end()) {
        return;
    }

    const RunningEntry& entry = it->second;
    std::unique_lock lock2(s_entriesMutex);

    ProfilerEntry& pEntry = s_entries[entry.scopeName];
    pEntry.offsets += stop - entry.start;
    pEntry.count++;

    s_runningEntries.erase(it);
#else
    (void)id;
#endif
}