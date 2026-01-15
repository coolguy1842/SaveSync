#include <Client.hpp>
#include <Config.hpp>
#include <Debug/Logger.hpp>
#include <FS/Directory.hpp>
#include <FS/File.hpp>
#include <Util/CURLEasy.hpp>
#include <Util/Defines.hpp>
#include <Util/StringUtil.hpp>
#include <format>
#include <list>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <set>

std::string Client::DownloadAction::actionKey(Client::DownloadAction::Action type) {
    switch(type) {
    case REPLACE: return "REPLACE";
    case CREATE:  return "CREATE";
    case REMOVE:  return "REMOVE";
    default:      return "KEEP";
    }
}

Client::DownloadAction::Action Client::DownloadAction::actionValue(std::string type) {
    switch(StringUtil::hash(type.c_str())) {
    case "REPLACE"_h: return REPLACE;
    case "CREATE"_h:  return CREATE;
    case "REMOVE"_h:  return REMOVE;
    default:          return KEEP;
    }
}

Result Client::emptyDownloadError() { return MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, RD_ALREADY_EXISTS); }
Result Client::finalizeDownloadError() { return MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, RD_NOT_AUTHORIZED); }
Result Client::beginDownload(std::shared_ptr<Title> title, Container container, std::string& ticket, std::vector<Client::DownloadAction>& fileActions) {
    Logger::info("Download Begin", "Starting Download for {:016X}, Container: {}", title->id(), getContainerName(container));

    std::vector<FileInfo> files = title->getContainerFiles(container);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

    writer.StartObject();
    {
        writer.Key("id");
        writer.Uint64(title->uniqueID());

        writer.Key("container");
        writer.String(getContainerName(container).c_str());

        writer.Key("existingFiles");
        writer.StartArray();

        for(const FileInfo& info : files) {
            writer.StartObject();

            writer.Key("path");
            writer.String(info.path.c_str());

            writer.Key("size");
            writer.Uint64(info.size);

            writer.Key("hash");
            printf("%s, hash: %s\n", info.path.c_str(), info.hash.value_or("").c_str());

            if(info.hash.has_value() || !info._shouldUpdateHash) {
                writer.String(info.hash->c_str());
            }
            else {
                writer.Null();
            }

            writer.EndObject();
        }

        writer.EndArray();
    }

    writer.EndObject();

    size_t jsonStrSize  = buf.GetSize();
    const char* jsonStr = buf.GetString();
    size_t jsonStrPos   = 0;

    rapidjson::Document document;
    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/download/begin", url(), ticket),
        .method         = POST,
        .contentType    = "application/json",
        .connectTimeout = 5,

        .read = ReadOptions{
            .dataSize = static_cast<long>(jsonStrSize),
            .callback = [&jsonStr, &jsonStrPos, &jsonStrSize](char* data, size_t dataSize) noexcept -> size_t {
                if(jsonStrPos >= jsonStrSize) {
                    return 0;
                }

                size_t read = std::min(jsonStrSize - jsonStrPos, dataSize);
                memcpy(data, jsonStr + jsonStrPos, read);

                jsonStrPos += read;
                return read;
            },
        },
        .write = WriteOptions{
            .callback = [&document](char* data, size_t dataSize) noexcept -> size_t {
                rapidjson::StringStream stream(data);
                document.ParseStream(stream);

                return dataSize;
            },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Download Begin", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }

    switch(easy.statusCode()) {
    case 200: break;
    case 204:
        Logger::info("Download Begin", "Status code is 204, stopping download early");
        return emptyDownloadError();
    default:
        Logger::warn("Download Begin", "Invalid status code: {} != 200", easy.statusCode());
        return invalidStatusCodeError();
    }

    if(document.HasParseError() || !document.IsObject() || NotType(document, "ticket", String) || NotType(document, "files", Array)) {
        Logger::warn("Download Begin", "Invalid JSON Document");
        return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    for(const auto& file : document["files"].GetArray()) {
        if(!file.IsObject() || NotType(file, "path", String) || NotType(file, "action", String) || (Exists(file, "size") && NotType(file, "size", Uint64)) || (Exists(file, "hash") && NotType(file, "hash", String))) {
            Logger::warn("Download Begin", "Invalid File JSON");

            fileActions.clear();
            return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_COMBINATION);
        }

        std::optional<u64> size = std::nullopt;
        if(Exists(file, "size")) {
            size = static_cast<u64>(file["size"].GetUint64());
        }

        std::optional<std::string> hash = std::nullopt;
        if(Exists(file, "hash")) {
            hash = static_cast<std::string>(file["hash"].GetString());
        }

        fileActions.push_back(DownloadAction{
            .path   = std::string(file["path"].GetString(), file["path"].GetStringLength()),
            .action = DownloadAction::actionValue(std::string(file["action"].GetString(), file["action"].GetStringLength())),
            .size   = size,
            .hash   = hash,
        });
    }

    ticket = std::string(document["ticket"].GetString(), document["ticket"].GetStringLength());
    return RL_SUCCESS;
}

Result Client::downloadFile(const std::string& ticket, std::shared_ptr<File> file, const std::string& path) {
    Logger::info("Download File", "Ticket: {} - Downloading {}", ticket, path);

    u64 fileWriteOffset = 0;
    CURLEasy easy;

    easy.setOptions({
        .url            = std::format("{}/v1/download/{}/file?path={}", url(), ticket, easy.escape(path)),
        .method         = GET,
        .connectTimeout = 2,

        .lowSpeed = LowSpeedOptions{
            .limit = 0,
            .time  = 5,
        },

        .write = WriteOptions{
            .bufferSize = 0x100,
            .callback   = [this, file, path, &fileWriteOffset](char* data, size_t dataSize) {
                u64 wrote = file->write(data, dataSize, fileWriteOffset);
                if(wrote == 0 || wrote == U64_MAX) {
                    Logger::warn("Download File", "Invalid write: {} size: {}", path, wrote);
                    return static_cast<size_t>(CURL_READFUNC_ABORT);
                }

                m_progressCurrent += wrote;
                fileWriteOffset += wrote;

                return static_cast<size_t>(wrote);
            },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Download File", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }
    else if(easy.statusCode() != 200) {
        Logger::warn("Download File", "Invalid status code: {} != 200", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

Result Client::endDownload(const std::string& ticket) {
    Logger::info("Download End", "Ticket: {} - Ending", ticket);

    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/download/{}", url(), ticket),
        .method         = DELETE,
        .noBody         = true,
        .connectTimeout = 5,
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Download End", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }
    else if(easy.statusCode() != 204) {
        Logger::warn("Download End", "Invalid status code: {} != 204", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

// TODO: v2 migration, see upload function
Result Client::download(std::shared_ptr<Title> title) {
    if(title == nullptr || !title->valid()) {
        Logger::error("Download", "Invalid title");
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    title->reloadContainerFiles(SAVE);
    title->reloadContainerFiles(EXTDATA);

    struct ContainerInfo {
        Container id;

        std::shared_ptr<ScopedLock> lock;
        std::shared_ptr<Archive> archive;

        std::vector<DownloadAction> fileActions;
        std::vector<FileInfo> newFiles;
        std::string ticket;

        bool reloadHash = false;
    };

    Result res     = RL_SUCCESS;
    bool bothEmpty = true;

    std::vector<ContainerInfo> containers;

    u64 totalTransferSize = 0;
    for(const Container& container : { SAVE, EXTDATA }) {
        if(!title->containerAccessible(container)) {
            continue;
        }

        ContainerInfo info = {
            .id      = container,
            .lock    = std::make_shared<ScopedLock>(title->containerMutex(container)),
            .archive = title->openContainer(container),
        };

        if(info.archive == nullptr || !info.archive->valid()) {
            continue;
        }

        // if nothing then skip container, mark as invalid
        Result beginRes;
        if(R_FAILED(beginRes = beginDownload(title, container, info.ticket, info.fileActions))) {
            if(beginRes != emptyDownloadError()) {
                beginRes = res;
                return res;
            }

            continue;
        }

        bothEmpty = false;
        for(const auto& file : info.fileActions) {
            switch(file.action) {
            case DownloadAction::REPLACE:
            case DownloadAction::CREATE:
                totalTransferSize += file.size.value_or(1);
                break;
            default: break;
            }
        }

        containers.push_back(info);
    }

    if(bothEmpty) {
        res = emptyDownloadError();
    }

    if(R_FAILED(res)) {
    cancelExit:
        for(const ContainerInfo& container : containers) {
            Logger::info("Download", "Cancelling download for container: {}", getContainerName(container.id));
            endDownload(container.ticket);
        }

        return res;
    }

    m_progressMax     = totalTransferSize;
    m_progressCurrent = 0;

    for(ContainerInfo& container : containers) {
        for(const auto& fileAction : container.fileActions) {
            switch(fileAction.action) {
            case DownloadAction::REPLACE: {
                auto file = container.archive->openFile(fileAction.path, FS_OPEN_WRITE, 0);
                if(file == nullptr || !file->valid()) {
                    Logger::warn("Download Replace", "Invalid file: {}", fileAction.path);

                    res = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, RD_INVALID_SELECTION);
                    goto cancelExit;
                }

                u64 size = file->size();
                if(size == U64_MAX) {
                    Logger::warn("Download Replace", "Failed to get file size: {}", fileAction.path);

                    res = file->lastResult();
                    goto cancelExit;
                }

                if(size != fileAction.size) {
                    switch(container.id) {
                    case EXTDATA: {
                        u32 attributes = file->attributes();
                        if(attributes == UINT32_MAX) {
                            attributes = FS_ATTRIBUTE_ARCHIVE;
                        }

                        file.reset();
                        if(!container.archive->deleteFile(fileAction.path)) {
                            Logger::warn("Download Replace", "Failed to delete old file: {}", fileAction.path);
                            res = file->lastResult();

                            goto cancelExit;
                        }

                        if(!container.archive->createFile(fileAction.path, fileAction.size.value_or(1), attributes)) {
                            Logger::warn("Download Replace", "Failed to create new file: {}", fileAction.path);
                            res = file->lastResult();

                            goto cancelExit;
                        }

                        file = container.archive->openFile(fileAction.path, FS_OPEN_WRITE, 0);
                        if(file == nullptr || !file->valid()) {
                            Logger::warn("Download Replace", "Invalid file after recreating: {}", fileAction.path);

                            res = MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, RD_INVALID_SELECTION);
                            goto cancelExit;
                        }

                        break;
                    }
                    default:
                        if(!file->setSize(fileAction.size.value_or(1))) {
                            Logger::warn("Download Replace", "Failed to set file size: {}", fileAction.path);

                            res = file->lastResult();
                            goto cancelExit;
                        }

                        break;
                    }
                }
            }
            // fall through
            case DownloadAction::CREATE: {
                if(!fileAction.size.has_value()) {
                    Logger::warn("Download Create", "No size for file action: {}", fileAction.path);

                    res = MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
                    goto cancelExit;
                }

                if(fileAction.action == DownloadAction::CREATE) {
                    auto it = fileAction.path.find_last_of("/");
                    if(it == std::string::npos) {
                        Logger::warn("Download Create", "No / present in path: {}", fileAction.path);

                        res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SELECTION);
                        goto cancelExit;
                    }

                    std::string dirPath = fileAction.path.substr(0, it);
                    if(it != 0 && !container.archive->mkdir(StringUtil::fromUTF8(dirPath), 0, true)) {
                        Logger::warn("Download Create", "Failed to mkdir path: {}", dirPath);

                        res = container.archive->lastResult();
                        goto cancelExit;
                    }

                    if(!container.archive->createFile(fileAction.path, fileAction.size.value_or(1), FS_ATTRIBUTE_ARCHIVE)) {
                        Logger::warn("Download Replace", "Failed to create new file: {}", fileAction.path);
                        res = container.archive->lastResult();

                        goto cancelExit;
                    }
                }

                auto file = container.archive->openFile(fileAction.path, FS_OPEN_WRITE, 0);
                if(file == nullptr || !file->valid()) {
                    Logger::warn("Download Create", "Failed to open path: {}", fileAction.path);

                    res = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, RD_INVALID_SELECTION);
                    goto cancelExit;
                }

                if(R_FAILED(res = downloadFile(container.ticket, file, fileAction.path))) {
                    Logger::warn("Download Create", "Failed to set download file: {}", fileAction.path);
                    goto cancelExit;
                }

                if(!file->flush()) {
                    Logger::warn("Download Create", "Failed to flush file: {}", fileAction.path);

                    res = file->lastResult();
                    goto cancelExit;
                }

                break;
            }
            case DownloadAction::REMOVE: {
                if(!container.archive->deleteFile(fileAction.path)) {
                    Logger::warn("Download Remove", "Failed to delete file: {}", fileAction.path);

                    res = container.archive->lastResult();
                    goto cancelExit;
                }

                continue;
            }
            default: break;
            }

            if(!fileAction.hash.has_value()) {
                container.reloadHash = true;
            }

            container.newFiles.push_back(FileInfo{
                .path = fileAction.path,

                .hash = fileAction.hash,
                .size = fileAction.size.value_or(1),

                ._shouldUpdateHash = fileAction.hash.has_value(),
            });
        }
    }

    bool updateHash = false;

    res = RL_SUCCESS;
    for(const ContainerInfo& container : containers) {
        if(container.reloadHash) {
            updateHash = true;
        }

        Result endRes;
        if(R_FAILED(endRes = endDownload(container.ticket))) {
            Logger::warn("Download", "Failed to end for {}, not treating as error", getContainerName(container.id));
            Logger::warn("Download", endRes);
        }

        if(container.id == Container::SAVE) {
            if(!container.archive->commitSaveData()) {
                Logger::warn("Download", "Failed to commit save data");
                Logger::warn("Download", container.archive->lastResult());

                res = finalizeDownloadError();

                continue;
            }

            if(R_FAILED(res = title->deleteSecureSaveValue())) {
                Logger::error("Download", "Failed to delete secure save value");

                res = finalizeDownloadError();
            }
        }

        auto infoLock = m_cachedTitleInfoMutex.lock();
        title->setContainerFiles(container.newFiles, container.id, false);
    }

    containers.clear();
    title->updateCache();

    if(updateHash) {
        m_titleLoader->reloadHashes();
    }
    else {
        titleCacheChangedSignal();
    }

    return RL_SUCCESS;
}