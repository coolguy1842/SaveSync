#include <FS/Archive.hpp>
#include <FS/Directory.hpp>
#include <Util/Logger.hpp>
#include <Util/Profiler.hpp>
#include <Util/StringUtil.hpp>
#include <string.h>

std::string Logger::formatResult(Result res) { return std::format("Result |lvl {}|sum {}|mod {}|desc {}|", R_LEVEL(res), R_SUMMARY(res), R_MODULE(res), R_DESCRIPTION(res)); }
const char* Logger::levelColor(Level level) {
    switch(level) {
    case INFO:     return infoColor; break;
    case WARN:     return warnColor; break;
    case ERROR:    return errorColor; break;
    case CRITICAL: return criticalColor; break;
    default:       return "";
    }
}

const char* Logger::levelName(Level level) {
    switch(level) {
    case INFO:     return "INFO"; break;
    case WARN:     return "WARNING"; break;
    case ERROR:    return "ERROR"; break;
    case CRITICAL: return "CRITICAL"; break;
    default:       return "";
    }
}

struct RenameEntry {
    bool doRename = false;

    std::u16string oldName;
    std::u16string newName;
};

bool Logger::s_dirInitialized  = false;
bool Logger::s_dirExists       = false;
std::mutex Logger::s_fileMutex = std::mutex();

std::shared_ptr<File> Logger::openLogFile() {
    const u8 maxLogs = 5;

    if(!s_dirInitialized) {
        s_dirInitialized              = true;
        std::shared_ptr<Archive> sdmc = Archive::open(ARCHIVE_SDMC, VarPath());
        if(sdmc == nullptr || !sdmc->valid() || !sdmc->mkdir(u"/3ds/SaveSync/logs", 0, true)) {
            Logger::error("Logger File", "Failed to create log folder");
            s_dirExists = false;

            return nullptr;
        }

        RenameEntry renames[maxLogs - 1];
        std::shared_ptr<Directory> dir = sdmc->openDirectory(u"/3ds/SaveSync/logs");

        for(const auto& entry : *dir) {
            if(!entry->isFile()) {
                continue;
            }

            const std::u16string& name = entry->name();
            if(!name.starts_with(u"log.") || !name.ends_with(u".txt")) {
                continue;
            }

            if(name == u"log.txt") {
                renames[0] = RenameEntry{
                    .doRename = true,

                    .oldName = entry->path(),
                    .newName = entry->prefix() + u"log.2.txt"
                };
            }
            else {
                size_t numOffset   = strlen("log.");
                char16_t logNumber = name[numOffset];

                std::u16string after = name.substr(numOffset + 1);
                if(logNumber < u'1' || logNumber > u'9' || after != u".txt") {
                    continue;
                }
                else if(logNumber - u'0' >= maxLogs) {
                    sdmc->deleteFile(entry->path());
                    continue;
                }

                std::u16string newName = entry->prefix() + u"log.";
                newName += logNumber + 1;
                newName += u".txt";

                renames[(logNumber - '0') - 1] = RenameEntry{
                    .doRename = true,

                    .oldName = entry->path(),
                    .newName = newName
                };
            }
        }

        for(u8 i = maxLogs - 2; i != UINT8_MAX; i--) {
            RenameEntry entry = renames[i];
            if(entry.doRename) {
                sdmc->renameFile(entry.oldName, entry.newName);
            }
        }

        s_dirExists = true;
    }
    else if(!s_dirExists) {
        return nullptr;
    }

    return File::openDirect(ARCHIVE_SDMC, VarPath(), "/3ds/SaveSync/logs/log.txt", FS_OPEN_CREATE | FS_OPEN_READ | FS_OPEN_WRITE, 0);
}

void Logger::fileLogMessage(const std::string& message) {
    if(message.empty()) {
        return;
    }

    std::unique_lock lock(s_fileMutex);
    std::shared_ptr<File> file = openLogFile();

    if(file != nullptr && file->valid()) {
        u32 size = file->size();
        if(size == UINT32_MAX || !file->setSize(size + message.size())) {
            return;
        }

        file->write(message.c_str(), message.size(), size, 0);
    }
}

void Logger::logProfiler() {
    std::vector<Profiler::AveragedEntry> entries = Profiler::getAverages();
    if(entries.empty()) {
        log("No Profiler Entries");
        return;
    }

    const std::string displayName = " Profiler Summary ";
    const size_t ticksWidth       = 14;

    size_t maxSize = std::max(strlen("scope") + 2, displayName.size() - ticksWidth + 3);
    for(const auto& entry : entries) {
        maxSize = std::max(maxSize, entry.scopeName.size() + 1);
    }

    // if even make odd
    if((maxSize & 1) == 0) {
        maxSize++;
    }

    std::string out;
    out += std::format("{:-^{}}\n", displayName, maxSize + ticksWidth + 3);
    out += std::format("|{:^{}}|{:^{}}|\n", "scope", maxSize, "milliseconds", ticksWidth);

    for(const auto& entry : entries) {
        out += std::format("|{:<{}}|{:<{}}|\n", entry.scopeName, maxSize, (entry.average / 268) / 1000.0f, ticksWidth);
    }

    out += std::format("{:-^{}}\n", "", maxSize + ticksWidth + 3);
    log("{}", out);
}
