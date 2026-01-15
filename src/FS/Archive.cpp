#include <FS/Archive.hpp>
#include <FS/Directory.hpp>
#include <FS/File.hpp>

Result invalidResult() { return MAKERESULT(RL_REINITIALIZE, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_HANDLE); }

static std::shared_ptr<Archive> s_sdmc;
void Archive::closeSDMC() { s_sdmc.reset(); }
std::shared_ptr<Archive> Archive::sdmc() {
    if(s_sdmc != nullptr) {
        return s_sdmc;
    }

    s_sdmc = Archive::open(ARCHIVE_SDMC, VarPath());
    return s_sdmc;
}

std::shared_ptr<Archive> Archive::open(FS_ArchiveID id, VarPath path) {
    struct make_shared_enabler : public Archive {
        make_shared_enabler(FS_ArchiveID id, FS_Path path)
            : Archive(id, path) {}
    };

    return std::make_shared<make_shared_enabler>(id, path.path);
}

Archive::Archive(FS_ArchiveID id, FS_Path path)
    : m_valid(false)
    , m_lastResult(RL_SUCCESS) {
    if(R_FAILED((m_lastResult = FSUSER_OpenArchive(&m_handle, id, path)))) {
        return;
    }

    m_valid = true;
}

Archive::~Archive() {
    if(!m_valid) {
        return;
    }

    FSUSER_CloseArchive(m_handle);
}

bool Archive::valid() const { return m_valid; }
FS_Archive Archive::handle() const { return m_handle; }
Result Archive::lastResult() const { return m_lastResult; }

#define CHECK_VALID(...)                                                                                \
    if(!m_valid) {                                                                                      \
        m_lastResult = MAKERESULT(RL_REINITIALIZE, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_HANDLE); \
        return __VA_ARGS__;                                                                             \
    }

bool Archive::mkdir(std::u16string path, u32 attributes, bool recursive) {
    CHECK_VALID(false)

    if(recursive) {
        size_t pos = 0;

        while((pos = path.find(u'/', pos + 1)) != std::u16string::npos) {
            if(R_FAILED(m_lastResult = FSUSER_CreateDirectory(m_handle, fsMakePath(PATH_UTF16, path.substr(0, pos).c_str()), attributes)) && R_SUMMARY(m_lastResult) != RS_NOP) {
                return false;
            }
        }
    }

    if(!recursive || !path.ends_with(u'/')) {
        m_lastResult = FSUSER_CreateDirectory(m_handle, fsMakePath(PATH_UTF16, path.c_str()), attributes);
    }
    else if(recursive) {
        m_lastResult = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_NO_DATA);
    }

    return R_SUCCEEDED(m_lastResult) || R_SUMMARY(m_lastResult) == RS_NOP;
}

bool Archive::createFile(VarPath path, u64 size, u32 attributes) {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_CreateFile(m_handle, path.path, attributes, size));
}

std::shared_ptr<Directory> Archive::openDirectory(std::u16string path) {
    CHECK_VALID(nullptr)

    return Directory::open(shared_from_this(), path);
}

std::shared_ptr<File> Archive::openFile(VarPath path, u32 flags, u32 attributes) {
    CHECK_VALID(nullptr)

    return File::open(shared_from_this(), path, flags, attributes);
}

bool Archive::renameDirectory(VarPath oldPath, VarPath newPath) {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_RenameDirectory(m_handle, oldPath.path, m_handle, newPath.path));
}

bool Archive::renameFile(VarPath oldPath, VarPath newPath) {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_RenameFile(m_handle, oldPath.path, m_handle, newPath.path));
}

bool Archive::deleteDirectory(VarPath path) {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_DeleteDirectory(m_handle, path.path));
}

bool Archive::deleteFile(VarPath path) {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_DeleteFile(m_handle, path.path));
}

bool Archive::commitSaveData() {
    CHECK_VALID(false)

    return R_SUCCEEDED(m_lastResult = FSUSER_ControlArchive(m_handle, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0));
}

bool Archive::hasDirectory(std::u16string path) {
    CHECK_VALID(false)

    auto dir = openDirectory(path);
    return dir != nullptr && dir->valid();
}

bool Archive::hasFile(VarPath path) {
    CHECK_VALID(false)

    auto file = openFile(path, FS_OPEN_READ, 0);
    return file != nullptr && file->valid();
}

#undef CHECK_VALID