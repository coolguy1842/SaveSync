#include <client.hpp>
#include <directory.hpp>
#include <httpHandler.hpp>
#include <settings.hpp>
#include <sstream>
#include <vector>

Client::Client() {}
Client::~Client() {}

bool Client::isServerOnline() {
    Result res = HttpHandler::instance()->sendRequest(ServerURL() + "/status");

    return R_SUCCEEDED(res);
}

TitleInfo Client::getSaveInfo(u64 titleID) {
    const TitleInfo emptySave = {
        titleID,
        { false, 0, "" },
        { false, 0, "" }
    };

    std::string temp;
    Result res = HttpHandler::instance()->sendRequest(ServerURL() + "/title/" + std::to_string(titleID), &temp);

    // printf("%d: %ld, %ld, %ld, %ld\n", R_SUCCEEDED(res), R_LEVEL(res), R_SUMMARY(res), R_MODULE(res), R_DESCRIPTION(res));
    if(R_FAILED(res) || temp.empty()) {
        return emptySave;
    }

    std::stringstream ss(temp);
    std::vector<std::string> split;

    while(std::getline(ss, temp, '\n')) {
        split.push_back(temp);
    }

    temp.clear();
    ss.clear();

    if(split.size() != 4) {
        return emptySave;
    }

    return {
        titleID,

        { !split[1].empty(), std::stoull(split[0]), split[1] },
        { !split[3].empty(), std::stoull(split[2]), split[3] }
    };
}

Result Client::uploadSaveDirectory(u64 titleID, FS_Archive& archive, std::u16string path) {
    Directory items(archive, path);

    if(!items.good()) {
        return items.error();
    }

    for(size_t i = 0; i < items.size(); i++) {
        std::u16string newPath = path + items.entry(i);

        if(items.folder(i)) {
            uploadSaveDirectory(titleID, archive, newPath + u"/");
        }
        else {
            std::stringstream ss;
            ss << ServerURL() << "/save/" << titleID << toUTF8(newPath);

            HttpHandler::instance()->sendRequest(
                ss.str(),
                nullptr,
                [&titleID, &archive, &newPath](httpcContext& context, const std::string& url, void* extraData) {
                    PutMethod(context, url, extraData);

                    Handle fileHandle;
                    FS_Path path = fsMakePath(PATH_UTF16, newPath.data());

                    Result res = FSUSER_OpenFile(&fileHandle, archive, path, FS_OPEN_READ, 0);
                    if(R_FAILED(res)) {
                        return res;
                    }

                    u64 size = 0;
                    if(R_FAILED((res = FSFILE_GetSize(fileHandle, &size)))) {
                        return res;
                    }

                    std::vector<u8> data(size);
                    u32 read = 0;

                    FSFILE_Read(fileHandle, &read, 0, data.data(), size);
                    if(R_FAILED((res = httpcAddPostDataRaw(&context, (u32*)data.data(), data.size() / 4)))) {
                        return res;
                    }

                    if(R_FAILED((res = FSFILE_Close(fileHandle)))) {
                        return res;
                    }

                    return res;
                }
            );

            printf("%s\n", toUTF8(newPath).c_str());
        }
    }

    return RL_SUCCESS;
}

void Client::uploadSave(std::shared_ptr<Title> title) {
    if(!title->accessibleSave()) {
        printf("can't upload save\n");
        return;
    }

    FS_Archive archive;
    Result res = Archive::save(&archive, title->mediaType(), title->lowID(), title->highID());

    if(R_SUCCEEDED(res)) {
        uploadSaveDirectory(title->id(), archive, u"/");
        FSUSER_CloseArchive(archive);
    }
}

std::string Client::ServerURL() const {
    return Settings::instance()->serverIP() + ":" + std::to_string(Settings::instance()->serverPort());
}