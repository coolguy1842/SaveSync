#include <FS/Archive.hpp>
#include <FS/Directory.hpp>
#include <FS/File.hpp>
#include <vector>

class DirectoryEntryPrivate {
public:
    ~DirectoryEntryPrivate() = default;
    DirectoryEntryPrivate(std::shared_ptr<Archive> archive, FS_DirectoryEntry entry, std::u16string prefix)
        : m_archive(archive)
        , m_entry(entry)
        , m_prefix(prefix) {
        if(m_prefix.size() != 0 && m_prefix.front() != u'/') {
            m_prefix.insert(m_prefix.begin(), u'/');
        }

        if(m_prefix.back() != u'/') {
            m_prefix += u'/';
        }
    }

    std::shared_ptr<Archive> m_archive;

    FS_DirectoryEntry m_entry;
    std::u16string m_prefix;
};

DirectoryEntry::DirectoryEntry(std::unique_ptr<DirectoryEntryPrivate>&& q) noexcept
    : q_ptr(std::move(q)) {}

std::u16string DirectoryEntry::prefix() const { return q_ptr->m_prefix; }
std::u16string DirectoryEntry::name() const { return reinterpret_cast<char16_t*>(q_ptr->m_entry.name); }
std::u16string DirectoryEntry::path() const {
    std::u16string path = prefix() + name();

    if(isDirectory() && path.back() != u'/') {
        path += u"/";
    }

    return path;
}

bool DirectoryEntry::isDirectory() const { return (q_ptr->m_entry.attributes & FS_ATTRIBUTE_DIRECTORY) != 0; }
bool DirectoryEntry::isFile() const { return !isDirectory(); }

bool DirectoryEntry::isHidden() const { return (q_ptr->m_entry.attributes & FS_ATTRIBUTE_HIDDEN) != 0; }
bool DirectoryEntry::isReadOnly() const { return (q_ptr->m_entry.attributes & FS_ATTRIBUTE_READ_ONLY) != 0; }

std::shared_ptr<Directory> DirectoryEntry::openDirectory() {
    if(!isDirectory()) {
        return nullptr;
    }

    return Directory::open(q_ptr->m_archive, path());
}

std::shared_ptr<File> DirectoryEntry::openFile(u32 flags, u32 attributes) {
    if(!isFile()) {
        return nullptr;
    }

    return File::open(q_ptr->m_archive, path(), flags, attributes);
}

std::shared_ptr<Directory> Directory::open(std::shared_ptr<Archive> archive, std::u16string path) {
    struct make_shared_enabler : public Directory {
        make_shared_enabler(std::shared_ptr<Archive> archive, std::u16string path)
            : Directory(archive, path) {}
    };

    return std::make_shared<make_shared_enabler>(archive, path);
}

Directory::Directory(std::shared_ptr<Archive> archive, std::u16string path)
    : m_valid(false)
    , m_path(path)
    , m_lastResult(RL_SUCCESS)
    , m_archive(archive) {
    if(archive == nullptr || !archive->valid()) {
        m_lastResult = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_HANDLE);
        return;
    }

    _reloadEntries();
    if(R_FAILED(m_lastResult)) {
        return;
    }

    m_valid = true;
}

bool Directory::valid() const { return m_valid; }
std::u16string Directory::path() const { return m_path; }

Result Directory::lastResult() const { return m_lastResult; }

void Directory::reloadEntries() {
    if(!m_valid) {
        m_lastResult = MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_HANDLE);
        return;
    }

    _reloadEntries();
}

void Directory::_reloadEntries() {
    m_entries.clear();

    Handle handle;
    if(R_FAILED((m_lastResult = FSUSER_OpenDirectory(&handle, m_archive->handle(), fsMakePath(PATH_UTF16, m_path.c_str()))))) {
        return;
    }

    const size_t maxEntries = 32;
    std::vector<FS_DirectoryEntry> entries(maxEntries);

    u32 entriesRead = 0;
    do {
        if(R_FAILED((m_lastResult = FSDIR_Read(handle, &entriesRead, entries.size(), entries.data())))) {
            m_entries.clear();
            FSDIR_Close(handle);

            return;
        }

        for(u32 i = 0; i < entriesRead; i++) {
            m_entries.push_back(std::make_shared<DirectoryEntry>(std::make_unique<DirectoryEntryPrivate>(m_archive, entries[i], m_path)));
        }
    } while(entriesRead == maxEntries);

    if(R_FAILED((m_lastResult = FSDIR_Close(handle)))) {
        m_entries.clear();
        return;
    }
}

std::vector<std::shared_ptr<DirectoryEntry>> Directory::entries() const { return m_entries; }
std::vector<std::shared_ptr<DirectoryEntry>>::const_iterator Directory::begin() const { return m_entries.begin(); }
std::vector<std::shared_ptr<DirectoryEntry>>::const_iterator Directory::end() const { return m_entries.end(); }