#ifndef __CLIENT_HPP__
#define __CLIENT_HPP__

#include <3ds.h>

#include <memory>
#include <string>
#include <title.hpp>

struct TitleInfo {
    // date 0 if doesnt exist
    // hash empty if doesnt exist
    struct FileData {
        bool exists;

        u64 date;
        std::string hash;
    };

    u64 titleID;

    FileData save;
    FileData extdata;
};

class Client {
public:
    Client();
    ~Client();

    bool isServerOnline();

    TitleInfo getSaveInfo(u64 titleID);
    void uploadSave(std::shared_ptr<Title> title);

private:
    Result uploadSaveDirectory(u64 titleID, FS_Archive& archive, std::u16string path);

    std::string ServerURL() const;
};

#endif