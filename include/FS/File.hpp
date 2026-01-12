#ifndef __FS_FILE_HPP__
#define __FS_FILE_HPP__

#include <3ds.h>

#include <FS/FSUtil.hpp>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Archive;
class File {
public:
    static std::shared_ptr<File> open(std::shared_ptr<Archive> archive, VarPath path, u32 flags, u32 attributes = 0);
    static std::shared_ptr<File> openDirect(FS_ArchiveID archiveID, VarPath archivePath, VarPath path, u32 flags, u32 attributes = 0);

    ~File();

    bool valid() const;
    Handle handle() const;

    FS_Path path() const;

    Result lastResult() const;

    // false if failed
    bool flush();
    bool setSize(u64 size);

    // U32_MAX if failed
    u32 attributes();

    // U64_MAX if failed
    u64 size();

    // UINT32_MAX if failed
    u32 write(const std::vector<u8>& data, u64 offset, u32 flags = 0);
    u32 write(const void* data, u64 dataSize, u64 offset, u32 flags = 0);

    std::vector<u8> read(u32 max, u64 offset);
    bool read(std::vector<u8>& data, u32 max, u64 offset);

    std::string readStr(u32 max, u64 offset);

    // U64_MAX if failed
    u64 read(void* data, u32 max, u64 offset);

    std::string readLine(u64 offset);

private:
    File(std::shared_ptr<Archive> archive, FS_Path path, u32 flags, u32 attributes);
    File(FS_ArchiveID archiveID, FS_Path archivePath, FS_Path path, u32 flags, u32 attributes);

    bool m_valid;
    Handle m_handle;

    FS_Path m_path;

    bool m_readOnly;
    bool m_hidden;

    Result m_lastResult;
    std::shared_ptr<Archive> m_archive;
};

#endif