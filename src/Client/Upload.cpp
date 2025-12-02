
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

Result Client::beginUpload(std::shared_ptr<Title> title, Container container, std::string& ticket, std::vector<std::string>& requestedFiles) {
    std::vector<FileInfo> files = title->getContainerFiles(container);
    if(files.size() <= 0) {
        Logger::warn("Upload Begin", "No files found for {}", getContainerName(container));
        return noFilesUploadError();
    }

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

    writer.StartObject();
    {
        writer.Key("id");
        writer.Uint64(title->id());

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
        .connectTimeout = 2,

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
    else if(easy.statusCode() == 204) {
        Logger::info("Upload Begin", "Status code is 204, stopping upload early");
        return emptyUploadError();
    }
    else if(easy.statusCode() != 200) {
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

        requestedFiles.push_back(std::string(file.GetString(), file.GetStringLength()));
    }

    ticket = std::string(document["ticket"].GetString(), document["ticket"].GetStringLength());
    return RL_SUCCESS;
}

Result Client::uploadFile(const std::string& ticket, std::shared_ptr<File> file, const std::string& path) {
    u64 fileSize = file->size();
    if(file->size() == U64_MAX) {
        Logger::warn("Upload File", "Failed to get file size: {}", path);
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    u64 fileReadOffset = 0;
    CURLEasy easy;

    easy.setOptions({
        .url            = std::format("{}/v1/upload/{}/file?path={}", url(), ticket, easy.escape(path)),
        .method         = PUT,
        .contentType    = "application/octet-stream",
        .connectTimeout = 2,

        .lowSpeed = LowSpeedOptions{
            .limit = 0,
            .time  = 5,
        },

        .read = ReadOptions{
            .bufferSize = 0x100,
            .dataSize   = static_cast<long>(fileSize),
            .callback   = [this, file, path, &fileSize, &fileReadOffset](char* data, size_t dataSize) {
                u64 read = file->read(reinterpret_cast<u8*>(data), dataSize, fileReadOffset);
                if(read == 0 || read == U64_MAX) {
                    Logger::warn("Upload File", "Invalid read: {} size: {}", path, read);
                    return static_cast<size_t>(CURL_READFUNC_ABORT);
                }

                fileReadOffset += read;
                m_progressCurrent += read;
                requestProgressChangedSignal(m_progressCurrent, m_progressMax);

                return static_cast<size_t>(read);
            },
        },
    });

    CURLcode code = easy.perform();
    setOnline(code == CURLE_OK);

    if(code != CURLE_OK) {
        Logger::warn("Upload File", "Invalid CURL code: {}", static_cast<int>(code));
        return performFailError();
    }
    else if(easy.statusCode() != 201 && easy.statusCode() != 204) {
        Logger::warn("Upload File", "Invalid status code: {} != 201 || 204", easy.statusCode());
        return invalidStatusCodeError();
    }

    return RL_SUCCESS;
}

Result Client::endUpload(const std::string& ticket) {
    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/upload/{}/end", url(), ticket),
        .method         = PUT,
        .noBody         = true,
        .connectTimeout = 2,

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
    CURLEasy easy(CURLEasyOptions{
        .url            = std::format("{}/v1/upload/{}", url(), ticket),
        .method         = DELETE,
        .noBody         = true,
        .connectTimeout = 2,
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

Result Client::upload(std::shared_ptr<Title> title, Container container) {
    if(title == nullptr || !title->valid()) {
        Logger::error("Upload", "Invalid title");
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_POINTER);
    }

    title->reloadContainerFiles(container);
    auto lock = title->containerMutex(container).lock();

    std::shared_ptr<Archive> archive = title->openContainer(container);
    if(archive == nullptr || !archive->valid()) {
        Logger::warn("Upload", "Invalid archive {}", getContainerName(container));
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_INVALID_HANDLE);
    }

    std::vector<std::string> requestedFiles;
    std::string ticket;

    Result res;
    if(R_FAILED(res = beginUpload(title, container, ticket, requestedFiles))) {
        if(res == emptyUploadError() || res == noFilesUploadError()) {
            return res;
        }

        Logger::warn("Upload", "Failed to begin");
        return res;
    }

    for(const std::string& path : requestedFiles) {
        std::shared_ptr<File> file = archive->openFile(path, FS_OPEN_READ, 0);
        if(file == nullptr || !file->valid()) {
            Logger::warn("Upload", "Invalid file: {}", path);

            res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SELECTION);
            goto cancelExit;
        }

        u64 size = file->size();
        if(size == U64_MAX) {
            Logger::warn("Upload", "Failed to get file size: {}", path);

            res = file->lastResult();
            goto cancelExit;
        }

        m_progressMax += size;
    }

    requestProgressChangedSignal(m_progressCurrent, m_progressMax);
    for(const std::string& path : requestedFiles) {
        std::shared_ptr<File> file = archive->openFile(path, FS_OPEN_READ, 0);
        if(file == nullptr || !file->valid()) {
            Logger::warn("Upload", "Invalid file: {}", path);

            res = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_SELECTION);
            goto cancelExit;
        }

        if(R_FAILED(res = uploadFile(ticket, file, path))) {
            Logger::warn("Upload", "Failed to upload file: {}", path);

            goto cancelExit;
        }
    }

    if(R_FAILED(res)) {
    cancelExit:
        cancelUpload(ticket);

        return res;
    }

    if(R_FAILED(res = endUpload(ticket))) {
        goto cancelExit;
    }

    auto infoLock = m_cachedTitleInfoMutex.lock();
    switch(container) {
    case SAVE:    m_cachedTitleInfo[title->id()].save = title->getContainerFiles(container); break;
    case EXTDATA: m_cachedTitleInfo[title->id()].extdata = title->getContainerFiles(container); break;
    default:      break;
    }

    title->setOutOfDate(title->outOfDate() ^ container);
    return RL_SUCCESS;
}