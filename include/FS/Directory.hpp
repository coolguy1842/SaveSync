#ifndef __FS_DIRECTORY_HPP__
#define __FS_DIRECTORY_HPP__

#include <3ds.h>

#include <memory>
#include <string>
#include <vector>

class Archive;
class Directory;
class File;

class DirectoryEntryPrivate;
class DirectoryEntry {
public:
    DirectoryEntry()                      = delete;
    DirectoryEntry(const DirectoryEntry&) = delete;
    ~DirectoryEntry()                     = default;

    DirectoryEntry(std::unique_ptr<DirectoryEntryPrivate>&& q) noexcept;

    std::u16string prefix() const;
    std::u16string name() const;
    std::u16string path() const;

    bool isDirectory() const;
    bool isFile() const;

    bool isHidden() const;
    bool isReadOnly() const;

    // these are nullable
    std::shared_ptr<Directory> openDirectory();
    std::shared_ptr<File> openFile(u32 flags, u32 attributes);

private:
    std::unique_ptr<DirectoryEntryPrivate> q_ptr;
};

class Directory {
public:
    static std::shared_ptr<Directory> open(std::shared_ptr<Archive> archive, std::u16string path);
    ~Directory();

    bool valid() const;
    std::u16string path() const;

    Result lastResult() const;
    void reloadEntries();

    std::vector<std::shared_ptr<DirectoryEntry>> entries() const;
    std::vector<std::shared_ptr<DirectoryEntry>>::const_iterator begin() const;
    std::vector<std::shared_ptr<DirectoryEntry>>::const_iterator end() const;

private:
    Directory(std::shared_ptr<Archive> archive, std::u16string path);

    bool m_valid;
    std::u16string m_path;

    Result m_lastResult;
    std::shared_ptr<Archive> m_archive;

    std::vector<std::shared_ptr<DirectoryEntry>> m_entries;
};

#endif