#include <Client.hpp>
#include <Config.hpp>
#include <Debug/Logger.hpp>
#include <Util/CURLEasy.hpp>
#include <Util/Defines.hpp>
#include <Util/StringUtil.hpp>
#include <format>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

constexpr std::optional<FileInfo> getFileInfo(const rapidjson::Value& val) {
    if(
        NotType(val, "path", String) ||
        NotType(val, "size", Uint64) ||
        NotType(val, "hash", String)
    ) {
        return std::nullopt;
    }

    return FileInfo{
        .path = std::format("/{}", val["path"].GetString()),

        .hash = val["hash"].GetString(),
        .size = val["size"].GetUint64(),
    };
}

bool Client::cachedTitleInfoLoaded() const { return m_titleInfoCached; }
std::unordered_map<u64, TitleInfo> Client::cachedTitleInfo() {
    auto lock = m_cachedTitleInfoMutex.lock();
    return m_cachedTitleInfo;
}

void Client::clearTitleInfoCache() {
    auto lock = m_cachedTitleInfoMutex.lock();

    m_titleInfoCached = false;
    m_cachedTitleInfo.clear();
}

Result Client::loadTitleInfoCache() {
    rapidjson::Document document;
    CURLEasy easy(CURLEasyOptions{
        .url    = std::format("{}/v1/titles", url()),
        .method = CURLEasyMethod::GET,

        .trackProgress = true,
        // 1 is abort
        .customProgressFunction = [this](curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept -> int {
            return m_requestWorker->waitingForExit();
        },
        .connectTimeout = 2,

        .write = WriteOptions{
            .callback = [&document](char* buf, size_t bufSize) noexcept -> size_t {
                rapidjson::StringStream stream(buf);
                document.ParseStream(stream);

                return bufSize;
            },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        switch(code) {
        case CURLE_URL_MALFORMAT:
        case CURLE_COULDNT_CONNECT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_ABORTED_BY_CALLBACK:  break;
        default:
            Logger::warn("Title Info", "Invalid CURL code: {}", static_cast<int>(code));
            break;
        }

        return performFailError();
    }
    else if(easy.statusCode() != 200) {
        Logger::warn("Title Info", "Invalid status code: {} != 200", easy.statusCode());
        return invalidStatusCodeError();
    }

    if(document.HasParseError() || !document.IsObject()) {
        Logger::warn("Title Info", "Invalid document: code: {} at {}", static_cast<u64>(document.GetParseError()), document.GetErrorOffset());
        return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    std::unordered_map<u64, TitleInfo> newCache;
    std::vector<u64> changed;
    std::vector<u64> removed;

    for(const auto& titleInfo : document.GetObject()) {
        const char* name                   = titleInfo.name.GetString();
        const rapidjson::SizeType nameSize = titleInfo.name.GetStringLength();

        if(nameSize <= 0 || !titleInfo.value.IsObject() || NotType(titleInfo.value, "save", Array) || NotType(titleInfo.value, "extdata", Array)) {
            continue;
        }

        char* endPtr;
        unsigned long long int title = std::strtoull(name, &endPtr, 10);
        if(title == 0 || title == ULLONG_MAX || endPtr != name + nameSize) {
            continue;
        }

        TitleInfo info;
        for(const auto& save : titleInfo.value["save"].GetArray()) {
            std::optional<FileInfo> fileInfo = getFileInfo(save);
            if(!fileInfo.has_value()) {
                continue;
            }

            info.save.push_back(fileInfo.value());
        }

        for(const auto& extdata : titleInfo.value["extdata"].GetArray()) {
            std::optional<FileInfo> fileInfo = getFileInfo(extdata);
            if(!fileInfo.has_value()) {
                continue;
            }

            info.extdata.push_back(fileInfo.value());
        }

        std::sort(info.save.begin(), info.save.end());
        std::sort(info.extdata.begin(), info.extdata.end());

        newCache[title] = info;
        if(!m_cachedTitleInfo.contains(title)) {
            changed.push_back(title);
            continue;
        }

        const TitleInfo& oldInfo = m_cachedTitleInfo[title];
        if(
            !std::equal(oldInfo.save.begin(), oldInfo.save.end(), info.save.begin(), info.save.end()) ||
            !std::equal(oldInfo.extdata.begin(), oldInfo.extdata.end(), info.extdata.begin(), info.extdata.end())
        ) {
            changed.push_back(title);
        }
    }

    for(auto entry : m_cachedTitleInfo) {
        if(newCache.contains(entry.first)) {
            continue;
        }

        removed.push_back(entry.first);
    }

    if(!changed.empty() || !removed.empty()) {
        m_titleInfoCached = true;

        {
            auto lock = m_cachedTitleInfoMutex.lock();
            m_cachedTitleInfo.swap(newCache);
        }

        titleCacheChangedSignal();

        for(auto title : changed) {
            titleInfoChangedSignal(title, m_cachedTitleInfo[title]);
        }

        for(auto title : removed) {
            TitleInfo info;
            titleInfoChangedSignal(title, info);
        }
    }
    else if(!m_titleInfoCached) {
        m_titleInfoCached = true;
        titleCacheChangedSignal();
    }

    return RL_SUCCESS;
}