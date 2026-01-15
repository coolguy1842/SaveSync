#ifndef __TITLE_HPP__
#define __TITLE_HPP__

#include <3ds.h>
#include <citro2d.h>

#include <FS/Archive.hpp>
#include <Util/Mutex.hpp>
#include <Util/SMDH.hpp>
#include <Util/TexWrapper.hpp>
#include <Util/Worker.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#define TITLE_ICON_WIDTH  48.0f
#define TITLE_ICON_HEIGHT 48.0f

#define TITLE_CACHE_VER "002"

enum Container {
    SAVE    = 0b01,
    EXTDATA = 0b10
};

struct FileInfo {
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

    bool invalidHash() const;

    u64 totalContainerSize(Container container);
    char* totalSizeStr();

    u64 id() const;
    u32 lowID() const;
    u32 highID() const;
    u32 uniqueID() const;

    u32 extdataID() const;

    const char* productCode() const;

    FS_MediaType mediaType() const;
    FS_CardType cardType() const;

    std::string name() const;
    const char* staticName() const;

    C2D_Image* icon();

    std::shared_ptr<Archive> openContainer(Container container) const;
    Mutex& containerMutex(Container container);

    bool containerAccessible(Container container) const;

    void resetContainerFiles(Container container);
    void reloadContainerFiles(Container container);
    std::vector<FileInfo> getContainerFiles(Container container) const;

    void updateCache();
    void setContainerFiles(const std::vector<FileInfo>& files, Container container, bool updateCache = true);
    void hashContainer(Container container, std::shared_ptr<u8> buf = nullptr, size_t bufSize = 0x1000, Worker* worker = nullptr);

    Result deleteSecureSaveValue();

    bool isOutOfSync() const;
    u8 outOfSync() const;
    void setOutOfSync(u8 outOfSync);

private:
    std::vector<FileInfo>& containerFiles(Container container);
    u64& totalSize(Container container);
    void updateTotalSizes();

    // to be run with a worker in the background
    void loadContainerFiles(Container container, bool cache = true, std::shared_ptr<Archive> archive = nullptr, bool lock = true);

    bool loadSMDHData();

    bool loadCache();
    void saveCache(bool lockCache = true, bool lockContainer = true);

private:
    bool m_valid;
    bool m_saveAccessible;
    bool m_extdataAccessible;

    bool m_invalidHash;

    // uses used size only, see FileInfo
    u64 m_totalSaveSize;
    u64 m_totalExtdataSize;

    char m_totalSizeStr[12];

    Mutex m_cacheMutex;
    Mutex m_saveMutex;
    Mutex m_extdataMutex;
    Mutex m_totalSizeMutex;

    u64 m_id;
    FS_MediaType m_mediaType;
    FS_CardType m_cardType;

    char m_productCode[16];

    std::vector<FileInfo> m_saveFiles;
    std::vector<FileInfo> m_extdataFiles;

    char m_longDescription[sizeof(SMDH::ApplicationTitle::longDescription)];

    std::shared_ptr<TexWrapper> m_tex;
    C2D_Image m_icon = { nullptr, nullptr };

    u8 m_outOfSync;
};

#endif