#ifndef __FS_ARCHIVE_HPP__
#define __FS_ARCHIVE_HPP__

#include <3ds.h>

#include <FS/FSUtil.hpp>
#include <memory>
#include <string>

#define DATA_DIRECTORY   "/3ds/" EXE_NAME
#define DATA_DIRECTORY_U u"/3ds/" EXE_NAME

class Directory;
class File;
class Archive : public std::enable_shared_from_this<Archive> {
public:
    static std::shared_ptr<Archive> sdmc();
    static void closeSDMC();

    static std::shared_ptr<Archive> open(FS_ArchiveID id, VarPath path);
    ~Archive();

    bool valid() const;
    FS_Archive handle() const;

    Result lastResult() const;

    bool mkdir(std::u16string path, u32 attributes, bool recursive = false);
    bool createFile(VarPath path, u64 size, u32 attributes);

    std::shared_ptr<Directory> openDirectory(std::u16string path = u"/");
    std::shared_ptr<File> openFile(VarPath path, u32 flags, u32 attributes);

    bool hasDirectory(std::u16string path);
    bool hasFile(VarPath path);

    bool renameDirectory(VarPath oldPath, VarPath newPath);
    bool renameFile(VarPath oldPath, VarPath newPath);

    bool deleteDirectory(VarPath path);
    bool deleteFile(VarPath path);

    bool commitSaveData();

private:
    Archive(FS_ArchiveID id, FS_Path path);

    bool m_valid;
    FS_Archive m_handle;

    Result m_lastResult;
};

#endif