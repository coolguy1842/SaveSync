#ifndef __TITLE_HPP__
#define __TITLE_HPP__

#include <3ds.h>
#include <citro2d.h>

#include <FS/Archive.hpp>
#include <Util/Mutex.hpp>
#include <Util/SMDH.hpp>
#include <Util/TexWrapper.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#define TITLE_ICON_WIDTH  48.0f
#define TITLE_ICON_HEIGHT 48.0f

enum Container {
    SAVE    = 0b01,
    EXTDATA = 0b10
};

struct FileInfo {
    std::u16string nativePath;
    std::string path;

    std::optional<std::string> hash = std::nullopt;
    u64 size;

    bool _shouldUpdateHash = false;

    bool operator<(const FileInfo& other) const {
        return path < other.path;
    }

    bool operator==(const FileInfo& other) const {
        return path == other.path &&
               size == other.size &&
               hash == other.hash;
    }
};

std::string getContainerName(Container container);

class Title {
public:
    Title(u64 id, FS_MediaType mediaType, FS_CardType cardType);
    ~Title();

    bool valid() const;
    // used by titleloader for game cards, DO NOT USE MANUALLY
    void setInvalid();

    u64 id() const;
    u32 lowID() const;
    u32 highID() const;
    u32 uniqueID() const;

    u32 extdataID() const;

    const char* productCode() const;

    FS_MediaType mediaType() const;
    FS_CardType cardType() const;

    std::string shortDescription() const;
    std::string longDescription() const;

    C2D_Image* icon();

    std::shared_ptr<Archive> openContainer(Container container) const;
    Mutex& containerMutex(Container container);

    bool containerAccessible(Container container) const;

    void resetContainerFiles(Container container);
    void reloadContainerFiles(Container container);
    std::vector<FileInfo> getContainerFiles(Container container) const;

    void setContainerFiles(std::vector<FileInfo>& files, Container container);
    void hashContainer(Container container);

    Result deleteSecureSaveValue();

    // bitmask of container
    u8 outOfDate() const;
    void setOutOfDate(u8 outOfDate);

private:
    std::vector<FileInfo>& containerFiles(Container container);

    // to be run with a worker in the background
    void loadContainerFiles(Container container, bool cache = true, std::shared_ptr<Archive> archive = nullptr, bool lock = true);

    bool loadSMDHData();

    bool loadCache();
    void saveCache();

private:
    bool m_valid;
    bool m_saveAccessible;
    bool m_extdataAccessible;

    Mutex m_saveMutex;
    Mutex m_extdataMutex;
    Mutex m_cacheMutex;

    u64 m_id;
    FS_MediaType m_mediaType;
    FS_CardType m_cardType;

    char m_productCode[16];

    std::vector<FileInfo> m_saveFiles;
    std::vector<FileInfo> m_extdataFiles;

    std::string m_shortDescription;
    std::string m_longDescription;

    std::shared_ptr<TexWrapper> m_tex;
    C2D_Image m_icon = { nullptr, nullptr };

    u8 m_outOfDate;
};

#endif