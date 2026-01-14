
#include <format>

#include <Client.hpp>
#include <Config.hpp>
#include <Debug/Logger.hpp>
#include <FS/File.hpp>
#include <Util/CURLEasy.hpp>
#include <Util/Defines.hpp>
#include <Util/StringUtil.hpp>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>

Result Client::noFilesUploadError() { return MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, RD_CANCEL_REQUESTED); }
Result Client::emptyUploadError() { return MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, RD_ALREADY_EXISTS); }
Result Client::finalizeUploadError() { return MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, RD_NOT_AUTHORIZED); }

// TODO: upload total size, and used size, some extdata & save files have uninitialized data, which must be preserved, as some games use it to check random things
Result Client::beginUpload(std::shared_ptr<Title> title, Container container, std::string& ticket, std::set<std::string>& requestedFiles) {
    Logger::info("Upload Begin", "Starting upload for {:016X}, Container: {}", title->id(), getContainerName(container));

    std::vector<FileInfo> files = title->getContainerFiles(container);
    if(files.size() <= 0) {
        Logger::info("Upload Begin", "No files found for {}", getContainerName(container));
        return noFilesUploadError();
    }

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

    writer.StartObject();
    {
        writer.Key("id");
        writer.Uint64(title->uniqueID());

        writer.Key("container");
        writer.String(getContainerName(container).c_str());

        writer.Key("files");
        writer.StartArray();

        for(const FileInfo& info : files) {
            writer.StartObject();

            writer.Key("path");
            writer.String(info.path.c_str());

            writer.Key("size");
            writer.Uint64(info.size);

            writer.Key("hash");

            if(info.hash.has_value() && !info._shouldUpdateHash) {
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
        .url            = std::format("{}/v1/upload/begin", url(), ticket),
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
        Logger::warn("Upload Begin", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }

    switch(easy.statusCode()) {
    case 200: break;
    case 204:
        Logger::info("Upload Begin", "Status code is 204, stopping upload early");
        return emptyUploadError();
    default:
        Logger::warn("Upload Begin", "Invalid status code: {} != 200", easy.statusCode());
        return invalidStatusCodeError();
    }

    if(document.HasParseError() || !document.IsObject() || NotType(document, "ticket", String) || NotType(document, "files", Array)) {
        Logger::warn("Upload Begin", "Invalid JSON Document");
        return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_RESULT_VALUE);
    }

    for(const auto& file : document["files"].GetArray()) {
        if(!file.IsString()) {
            Logger::warn("Upload Begin", "Invalid JSON file entry");

            requestedFiles.clear();
            return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_COMBINATION);
        }

        requestedFiles.emplace(std::string(file.GetString(), file.GetStringLength()));
    }

    ticket = std::string(document["ticket"].GetString(), document["ticket"].GetStringLength());
    return RL_SUCCESS;
}

// TODO: change to only upload real data
Result Client::uploadFile(const std::string& ticket, std::shared_ptr<File> file, const std::string& path) {
    Logger::info("Upload File", "Ticket: {} - Uploading {}", ticket, path);

    u64 fileSize = file->size();
    if(file->size() == U64_MAX) {
        Logger::warn("Upload File", "Failed to get file size: {}", path);
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    u64 fileReadOffset = 0;
    CURLEasy easy;

    bool uploadingFake = false;
    easy.setOptions({
        .url            = std::format("{}/v1/upload/{}/file?path={}", url(), ticket, easy.escape(path)),
        .method         = PUT,
        .contentType    = "application/octet-stream",
        .connectTimeout = 5,

        .lowSpeed = LowSpeedOptions{
            .limit = 0,
            .time  = 5,
        },

        .read = ReadOptions{
            .bufferSize = 0x8000,
            .dataSize   = static_cast<long>(fileSize),
            .callback   = [this, file, path, &fileSize, &fileReadOffset, &uploadingFake](char* data, size_t dataSize) {
                if(fileReadOffset >= fileSize) {
                    return 0U;
                }

                constexpr size_t readSize = 0x1000;
                size_t bytesRead          = 0;

                if(uploadingFake) {
                doFakeUpload:
                    size_t fakeSize = dataSize - bytesRead;

                    if(fileReadOffset + fakeSize > fileSize) {
                        fakeSize = fileSize - fileReadOffset;
                    }

                    memset(data + bytesRead, 0, fakeSize);
                    fileReadOffset += fakeSize;
                    m_progressCurrent += fakeSize;

                    return bytesRead + fakeSize;
                }

                for(; bytesRead < (readSize * round(dataSize / readSize));) {
                    u64 read = file->read(data + bytesRead, readSize, fileReadOffset);
                    if(read == U64_MAX) {
                        if(file->lastResult() == -0x26FFBA75) {
                            uploadingFake = true;
                            goto doFakeUpload;
                        }

                        return static_cast<size_t>(CURL_READFUNC_ABORT);
                    }
                    else if(read == 0) {
                        break;
                    }

                    bytesRead += read;
                    fileReadOffset += read;
                    m_progressCurrent += read;
                    requestProgressChangedSignal(m_progressCurrent, m_progressMax);

                    if(read < readSize) {
                        break;
                    }
                }

                return static_cast<size_t>(bytesRead);
            },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Upload File", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }

    switch(easy.statusCode()) {
    case 201:
    case 204: break;
    default:
        Logger::warn("Upload File", "Invalid status code: {} != 201 || 204", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

Result Client::endUpload(const std::string& ticket) {
    Logger::info("Upload End", "Ticket: {} - Ending", ticket);

    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/upload/{}/end", url(), ticket),
        .method         = PUT,
        .noBody         = true,
        .connectTimeout = 5,

        .read = ReadOptions{
            .dataSize = 0,
            .callback = [](char*, size_t) noexcept -> size_t { return 0; },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Upload End", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }
    else if(easy.statusCode() != 204) {
        Logger::warn("Upload End", "Invalid status code: {} != 204", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

Result Client::cancelUpload(const std::string& ticket) {
    Logger::info("Upload Cancel", "Ticket: {} - Cancelling", ticket);

    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/upload/{}", url(), ticket),
        .method         = DELETE,
        .noBody         = true,
        .connectTimeout = 5,
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Upload Cancel", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }
    else if(easy.statusCode() != 204) {
        Logger::warn("Upload Cancel", "Invalid status code: {} != 204", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

// TODO: migrate to v2, still must draw up docs for v2 API, should hopefully merge SAVE & EXTDATA uploading (not file structure) to make cancelling more safe
Result Client::upload(std::shared_ptr<Title> title) {
    if(title == nullptr || !title->valid()) {
        Logger::error("Upload", "Invalid Title");
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    title->reloadContainerFiles(SAVE);
    title->reloadContainerFiles(EXTDATA);

    struct ContainerInfo {
        Container id;

        std::shared_ptr<ScopedLock> lock;
        std::shared_ptr<Archive> archive;

        std::set<std::string> requestedFiles;
        std::string ticket;
    };

    Result res     = RL_SUCCESS;
    bool bothEmpty = true;

    std::vector<ContainerInfo> containers;

    u64 totalTransferSize = 0;
    for(const Container& container : { SAVE, EXTDATA }) {
        if(!title->containerAccessible(container)) {
            continue;
        }

        Logger::info("Upload", "Locking, and opening container: {}", getContainerName(container));
        ContainerInfo info = {
            .id      = container,
            .lock    = std::make_shared<ScopedLock>(title->containerMutex(container)),
            .archive = title->openContainer(container),
        };
        Logger::info("Upload", "Locked, and opened");

        if(info.archive == nullptr || !info.archive->valid()) {
            continue;
        }

        // if nothing then skip container, mark as invalid
        Result beginRes;
        if(R_FAILED(beginRes = beginUpload(title, container, info.ticket, info.requestedFiles))) {
            if(beginRes != noFilesUploadError() && beginRes != emptyUploadError()) {
                beginRes = res;
                return res;
            }

            continue;
        }

        bothEmpty = false;

        const auto& existingFiles = title->getContainerFiles(container);
        for(const auto& file : existingFiles) {
            if(!info.requestedFiles.contains(file.path)) {
                continue;
            }

            totalTransferSize += file.size;
        }

        containers.push_back(info);
    }

    if(bothEmpty) {
        res = noFilesUploadError();
    }

    if(R_FAILED(res)) {
    cancelExit:
        for(const ContainerInfo& container : containers) {
            Logger::info("Upload", "Cancelling upload for container: {}", getContainerName(container.id));
            cancelUpload(container.ticket);
        }

        return res;
    }

    m_progressMax     = totalTransferSize;
    m_progressCurrent = 0;

    for(const ContainerInfo& container : containers) {
        for(const std::string& path : container.requestedFiles) {
            std::shared_ptr<File> file = container.archive->openFile(path, FS_OPEN_READ, 0);
            if(file == nullptr || !file->valid()) {
                Logger::warn("Upload", "Invalid file: {}", path);

                res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SELECTION);
                goto cancelExit;
            }

            if(R_FAILED(res = uploadFile(container.ticket, file, path))) {
                Logger::warn("Upload", "Failed to upload file: {}", path);
                goto cancelExit;
            }
        }
    }

    res = RL_SUCCESS;
    for(const ContainerInfo& container : containers) {
        Result endRes;
        if(R_FAILED(endRes = endUpload(container.ticket))) {
            Logger::warn("Upload", "Failed to end for {}", getContainerName(container.id));
            res = finalizeUploadError();

            // TODO: unsafe for now, should cancel both save & extdata, but for now we can ignore it and just not update the hashes
            continue;
        }

        auto infoLock = m_cachedTitleInfoMutex.lock();
        switch(container.id) {
        case SAVE:    m_cachedTitleInfo[title->uniqueID()].save = title->getContainerFiles(container.id); break;
        case EXTDATA: m_cachedTitleInfo[title->uniqueID()].extdata = title->getContainerFiles(container.id); break;
        default:      break;
        }
    }

    titleCacheChangedSignal();
    titleInfoChangedSignal(title->id(), m_cachedTitleInfo[title->uniqueID()]);

    return res;
}