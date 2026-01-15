#include <FS/Archive.hpp>
#include <FS/File.hpp>

#include <algorithm>
#include <optional>

std::shared_ptr<File> File::open(std::shared_ptr<Archive> archive, VarPath path, u32 flags, u32 attributes) {
    struct make_shared_enabler : public File {
        make_shared_enabler(std::shared_ptr<Archive> archive, FS_Path path, u32 flags, u32 attributes)
            : File(archive, path, flags, attributes) {}
    };

    return std::make_shared<make_shared_enabler>(archive, path.path, flags, attributes);
}

std::shared_ptr<File> File::openDirect(FS_ArchiveID archiveID, VarPath archivePath, VarPath path, u32 flags, u32 attributes) {
    struct make_shared_enabler : public File {
        make_shared_enabler(FS_ArchiveID archiveID, FS_Path archivePath, FS_Path path, u32 flags, u32 attributes)
            : File(archiveID, archivePath, path, flags, attributes) {}
    };

    return std::make_shared<make_shared_enabler>(archiveID, archivePath.path, path.path, flags, attributes);
}

File::File(std::shared_ptr<Archive> archive, FS_Path path, u32 flags, u32 attributes)
    : m_valid(false)
    , m_path(path)
    , m_lastResult(RL_SUCCESS)
    , m_archive(archive) {
    if(R_FAILED((m_lastResult = FSUSER_OpenFile(&m_handle, m_archive->handle(), path, flags, attributes)))) {
        return;
    }

    m_valid = true;
}

File::File(FS_ArchiveID archiveID, FS_Path archivePath, FS_Path path, u32 flags, u32 attributes)
    : m_valid(false)
    , m_path(path)
    , m_lastResult(RL_SUCCESS) {
    if(R_FAILED((m_lastResult = FSUSER_OpenFileDirectly(&m_handle, archiveID, archivePath, path, flags, attributes)))) {
        return;
    }

    m_valid = true;
}

File::~File() {
    if(!m_valid) {
        return;
    }

    FSFILE_Close(m_handle);
}

bool File::valid() const { return m_valid; }
Handle File::handle() const { return m_handle; }

FS_Path File::path() const { return m_path; }

Result File::lastResult() const { return m_lastResult; }

#define CHECK_VALID(...)                                                                                \
    if(!m_valid) {                                                                                      \
        m_lastResult = MAKERESULT(RL_REINITIALIZE, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_HANDLE); \
        return __VA_ARGS__;                                                                             \
    }

u32 File::attributes() {
    CHECK_VALID(UINT32_MAX)

    u32 attributes;
    if(R_FAILED(m_lastResult = FSFILE_GetAttributes(m_handle, &attributes))) {
        return UINT32_MAX;
    }

    return attributes;
}

u64 File::read(void* data, u32 max, u64 offset) {
    CHECK_VALID(U64_MAX)

    u32 numRead = 0;
    if(R_FAILED((m_lastResult = FSFILE_Read(m_handle, &numRead, offset, data, max)))) {
        return U64_MAX;
    }

    m_lastResult = RL_SUCCESS;
    return numRead;
}

bool File::read(std::vector<u8>& data, u32 max, u64 offset) {
    CHECK_VALID(false)
    data.resize(max);

    u32 numRead = 0;
    if(R_FAILED((m_lastResult = FSFILE_Read(m_handle, &numRead, offset, data.data(), max)))) {
        return false;
    }

    data.resize(numRead);

    m_lastResult = RL_SUCCESS;
    return true;
}

std::string File::readStr(u32 max, u64 offset) {
    CHECK_VALID("")

    std::string out;
    out.resize(max);

    u32 numRead = 0;
    if(R_FAILED((m_lastResult = FSFILE_Read(m_handle, &numRead, offset, out.data(), max)))) {
        return "";
    }

    out.resize(numRead);
    m_lastResult = RL_SUCCESS;

    return out;
}

std::vector<u8> File::read(u32 max, u64 offset) {
    CHECK_VALID({})
    std::vector<u8> out;

    read(out, max, offset);
    return out;
}

std::string File::readLine(u64 offset) {
    CHECK_VALID("")

    const u64 bufSize = 128;

    u64 off = offset;
    std::vector<u8> buf;

    std::string out;
    std::vector<u8>::iterator it = buf.end();

    do {
        buf = read(bufSize, off);
        if(R_FAILED(m_lastResult)) {
            return "";
        }

        it = std::find(buf.begin(), buf.end(), '\n');
        out += std::string(buf.begin(), it);
    } while(buf.size() == bufSize && it == buf.end());

    return out;
}

u32 File::write(const std::vector<u8>& data, u64 offset, u32 flags) {
    CHECK_VALID(UINT32_MAX)

    u32 numWritten = 0;
    if(R_FAILED((m_lastResult = FSFILE_Write(m_handle, &numWritten, offset, data.data(), data.size(), flags)))) {
        return UINT32_MAX;
    }

    m_lastResult = RL_SUCCESS;
    return numWritten;
}

u32 File::write(const void* data, u64 dataSize, u64 offset, u32 flags) {
    CHECK_VALID(UINT32_MAX)

    u32 numWritten = 0;
    if(R_FAILED((m_lastResult = FSFILE_Write(m_handle, &numWritten, offset, data, dataSize, flags)))) {
        return UINT32_MAX;
    }

    m_lastResult = RL_SUCCESS;
    return numWritten;
}

bool File::flush() {
    CHECK_VALID(false)

    m_lastResult = RL_SUCCESS;
    return R_SUCCEEDED((m_lastResult = FSFILE_Flush(m_handle)));
}

bool File::setSize(u64 size) {
    CHECK_VALID(false)

    m_lastResult = RL_SUCCESS;
    return R_SUCCEEDED((m_lastResult = FSFILE_SetSize(m_handle, size)));
}

u64 File::size() {
    CHECK_VALID(U64_MAX)

    u64 size;
    if(R_FAILED((m_lastResult = FSFILE_GetSize(m_handle, &size)))) {
        return U64_MAX;
    }

    m_lastResult = RL_SUCCESS;
    return size;
}
