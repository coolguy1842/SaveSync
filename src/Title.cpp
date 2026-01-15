#include <Debug/Logger.hpp>
#include <Debug/Profiler.hpp>
#include <FS/Directory.hpp>
#include <FS/File.hpp>
#include <Title.hpp>
#include <Util/StringUtil.hpp>
#include <Util/Worker.hpp>
#include <iomanip>
#include <iostream>
#include <list>
#include <md5.h>
#include <sstream>

std::string getContainerName(Container container) {
    switch(container) {
    case SAVE:    return "SAVE";
    case EXTDATA: return "EXTDATA";
    default:      return "SAVE";
    }
}

constexpr std::shared_ptr<Archive> _save(FS_MediaType mediaType, u32 lowID, u32 highID) {
    if(mediaType == MEDIATYPE_NAND) {
        const u32 path[2] = { mediaType, (0x00020000 | lowID >> 8) };
        return Archive::open(ARCHIVE_SYSTEM_SAVEDATA, FS_Path{ PATH_BINARY, sizeof(path), path });
    }

    const u32 path[3] = { mediaType, lowID, highID };
    return Archive::open(ARCHIVE_USER_SAVEDATA, FS_Path{ PATH_BINARY, sizeof(path), path });
}

constexpr std::shared_ptr<Archive> _extdata(u32 id) {
    const u32 path[3] = { MEDIATYPE_SD, id, 0 };
    return Archive::open(ARCHIVE_EXTDATA, FS_Path{ PATH_BINARY, sizeof(path), path });
}

Title::Title(u64 id, FS_MediaType mediaType, FS_CardType cardType)
    : m_valid(false)
    , m_saveAccessible(false)
    , m_extdataAccessible(false)
    , m_invalidHash(false)
    , m_totalSaveSize(0)
    , m_totalExtdataSize(0)
    , m_id(id)
    , m_mediaType(mediaType)
    , m_cardType(cardType)
    , m_outOfSync(0) {
    PROFILE_SCOPE("Load Title");

    if((m_id & 0x000000FFFF000000) != 0) {
        // title is an update, only show base games
        return;
    }

    if(R_FAILED(AM_GetTitleProductCode(m_mediaType, m_id, m_productCode))) {
        strcpy(m_productCode, "Invalid");
    }

    {
        PROFILE_SCOPE("Load Title Accessibles");

        auto archive     = _save(m_mediaType, lowID(), highID());
        m_saveAccessible = archive != nullptr && archive->valid();

        archive             = _extdata(extdataID());
        m_extdataAccessible = archive != nullptr && archive->valid();

        if(!m_saveAccessible && !m_extdataAccessible) {
            return;
        }
    }

    m_valid = true;
    if(!loadCache()) {
        m_valid = false;
        return;
    }
}

Title::~Title() {
    m_icon = { nullptr, nullptr };
}

bool Title::valid() const { return m_valid; }
void Title::setInvalid() { m_valid = false; }

bool Title::invalidHash() const { return m_invalidHash; }

u64 Title::totalContainerSize(Container container) {
    auto lock = m_totalSizeMutex.lock();

    switch(container) {
    case SAVE: return m_totalSaveSize;
    default:   return m_totalExtdataSize;
    }
}

u64& Title::totalSize(Container container) {
    switch(container) {
    case SAVE: return m_totalSaveSize;
    default:   return m_totalExtdataSize;
    }
}

char* Title::totalSizeStr() {
    auto lock = m_totalSizeMutex.lock();
    return m_totalSizeStr;
}

void Title::updateTotalSizes() {
    auto lock = m_totalSizeMutex.lock();

    for(Container container : { SAVE, EXTDATA }) {
        u64& numTotalSize = totalSize(container);
        numTotalSize      = 0;

        for(const auto& file : containerFiles(container)) {
            numTotalSize += file.size;
        }
    }

    StringUtil::formatFileSize(m_totalSaveSize + m_totalExtdataSize, m_totalSizeStr, sizeof(m_totalSizeStr));
}

FS_MediaType Title::mediaType() const { return m_mediaType; }
FS_CardType Title::cardType() const { return m_cardType; }

u64 Title::id() const { return m_id; }
u32 Title::highID() const { return static_cast<u32>(m_id >> 32); }
u32 Title::lowID() const { return static_cast<u32>(m_id); }
u32 Title::uniqueID() const { return (lowID() & 0x0FFFFF00) >> 8; }

const char* Title::productCode() const { return m_productCode; }

std::string Title::name() const { return std::string(m_longDescription); }
const char* Title::staticName() const { return m_longDescription; }
C2D_Image* Title::icon() { return &m_icon; }

// from checkpoint
u32 Title::extdataID() const {
    u32 low = lowID();
    switch(low) {
    case 0x00055E00: return 0x055D; // Pokémon Y
    case 0x0011C400: return 0x11C5; // Pokémon Omega Ruby
    case 0x00175E00: return 0x1648; // Pokémon Moon
    case 0x00179600:
    case 0x00179800: return 0x1794; // Fire Emblem Conquest SE NA
    case 0x00179700:
    case 0x0017A800: return 0x1795; // Fire Emblem Conquest SE EU
    case 0x0012DD00:
    case 0x0012DE00: return 0x12DC; // Fire Emblem If JP
    case 0x001B5100:
        return 0x1B50; // Pokémon Ultramoon
                       // TODO: need confirmation for this
                       // case 0x001C5100:
                       // case 0x001C5300:
                       //     return 0x0BD3; // Etrian Odyssey V: Beyond the Myth
    default: return low >> 8;
    }
}

std::shared_ptr<Archive> Title::openContainer(Container container) const {
    if(!m_valid) return nullptr;

    switch(container) {
    case SAVE:
        if(!m_saveAccessible) {
            return nullptr;
        }

        return _save(m_mediaType, lowID(), highID());
    case EXTDATA:
        if(!m_extdataAccessible) {
            return nullptr;
        }

        return _extdata(extdataID());
    default: return nullptr;
    }
}

Mutex& Title::containerMutex(Container container) {
    switch(container) {
    case SAVE: return m_saveMutex;
    default:   return m_extdataMutex;
    }
}

bool Title::containerAccessible(Container container) const {
    if(!m_valid) return false;

    switch(container) {
    case SAVE:    return m_saveAccessible;
    case EXTDATA: return m_extdataAccessible;
    default:      return false;
    }
}

Result Title::deleteSecureSaveValue() {
    if(!m_valid) return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_INVALID_SELECTION);

    if(m_cardType != CARD_CTR) {
        Logger::warn("Title", "Can't delete secure save value for non CTR title: {:016X}", m_id);
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_NOT_FOUND);
    }

    u8 out;
    u64 secureValue = (static_cast<u64>(SECUREVALUE_SLOT_SD) << 32) | ((lowID() >> 8) << 8);
    return FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &secureValue, 8, &out, 1);
}

void Title::reloadContainerFiles(Container container) { loadContainerFiles(container); }
void Title::resetContainerFiles(Container container) {
    if(!m_valid) return;

    switch(container) {
    case SAVE:    m_saveFiles.clear(); break;
    case EXTDATA: m_extdataFiles.clear(); break;
    default:      return;
    }

    reloadContainerFiles(container);
}

std::vector<FileInfo> Title::getContainerFiles(Container container) const {
    if(!m_valid) return {};

    switch(container) {
    case SAVE:    return m_saveFiles;
    case EXTDATA: return m_extdataFiles;
    default:      return {};
    }
}

std::vector<FileInfo>& Title::containerFiles(Container container) {
    switch(container) {
    case SAVE: return m_saveFiles;
    default:   return m_extdataFiles;
    }
}

void Title::updateCache() {
    saveCache();
}

void Title::setContainerFiles(const std::vector<FileInfo>& files, Container container, bool updateCache) {
    if(!m_valid) return;

    switch(container) {
    case SAVE:
        m_saveFiles = files;
        std::sort(m_saveFiles.begin(), m_saveFiles.end());

        break;
    case EXTDATA:
        m_extdataFiles = files;
        std::sort(m_extdataFiles.begin(), m_extdataFiles.end());

        break;
    default: return;
    }

    if(updateCache) {
        saveCache();
    }
}

void Title::loadContainerFiles(Container container, bool cache, std::shared_ptr<Archive> archive, bool shouldLock) {
    if(!m_valid) return;

    ScopedLock lock = ScopedLock(containerMutex(container), !shouldLock);
    if(archive == nullptr) {
        archive = openContainer(container);
    }

    if(archive == nullptr || !archive->valid()) {
        Logger::warn("Load Title Container", "Archive invalid, title: {:016X} container: {}", m_id, getContainerName(container));
        return;
    }

    PROFILE_SCOPE("Load Title Container");
    std::vector<FileInfo>& files = containerFiles(container);
    std::unordered_map<std::string, FileInfo> oldFiles;

    for(const auto& file : files) {
        oldFiles.emplace(file.path, file);
    }

    std::vector<FileInfo> newFiles;
    std::list<std::shared_ptr<Directory>> directories = { archive->openDirectory() };
    while(directories.size() >= 1) {
        std::shared_ptr<Directory> dir = directories.front();
        if(dir == nullptr || !dir->valid()) {
            goto skip;
        }

        for(auto entry : *dir) {
            if(entry->isDirectory()) {
                directories.push_back(entry->openDirectory());
                continue;
            }
            else if(!entry->isFile()) {
                continue;
            }

            std::string path = StringUtil::toUTF8(entry->path());
            auto it          = oldFiles.find(path);
            if(it != oldFiles.end()) {
                newFiles.push_back(it->second);

                continue;
            }

            std::shared_ptr<File> file = entry->openFile(FS_OPEN_READ, 0);
            if(file == nullptr || !file->valid()) {
                continue;
            }

            u64 size = file->size();
            if(size == U64_MAX) {
                continue;
            }

            newFiles.push_back(FileInfo{
                .path = path,
                .size = size,

                ._shouldUpdateHash = false,
            });
        }

    skip:
        directories.pop_front();
    }

    std::sort(newFiles.begin(), newFiles.end());
    files.swap(newFiles);

    updateTotalSizes();

    if(cache) {
        if(shouldLock) {
            lock.release();
        }

        saveCache();
    }
}

void Title::hashContainer(Container container, std::shared_ptr<u8> buf, size_t bufSize, Worker* worker) {
    if(!m_valid || worker->waitingForExit()) return;

    auto lock                        = containerMutex(container).lock();
    std::shared_ptr<Archive> archive = openContainer(container);
    if(archive == nullptr || !archive->valid()) {
        return;
    }

    PROFILE_SCOPE("Hash Container");

    Logger::info("Title Hash Container", "Hashing {} for: {:016X}", getContainerName(container), m_id);
    const u64 start = svcGetSystemTick();

    loadContainerFiles(container, false, archive, false);
    std::vector<FileInfo>& files = containerFiles(container);

    if(buf == nullptr) {
        if(bufSize == 0) {
            bufSize = 0x1000;
        }

        buf.reset(reinterpret_cast<u8*>(malloc(bufSize)));
    }

    for(auto it = files.begin(); it != files.end();) {
        if(worker != nullptr && worker->waitingForExit()) {
            return;
        }

        FileInfo& info             = *it;
        std::shared_ptr<File> file = archive->openFile(info.path, FS_OPEN_READ, 0);

        u64 newSize;
        if(file == nullptr || !file->valid() || (newSize = file->size()) == U64_MAX) {
            it = files.erase(it);

            continue;
        }

        info.size = newSize;

        MD5Context ctx;
        md5Init(&ctx);

        u64 offset = 0, bytesRead;
        while(true) {
            if(worker != nullptr && worker->waitingForExit()) {
                m_invalidHash = true;
                return;
            }

            bytesRead = file->read(buf.get(), bufSize, offset);
            if(bytesRead == 0) {
                break;
            }

            if(bytesRead == U64_MAX) {
                // read all the initialized blocks
                // WARN: this assumes all valid data is contiguous
                if(file->lastResult() == -0x26FFBA75) {
                    memset(buf.get(), 0, bufSize);

                    for(; offset < info.size; offset += bufSize) {
                        if(offset + bufSize > info.size) {
                            md5Update(&ctx, buf.get(), info.size - offset);
                            break;
                        }

                        md5Update(&ctx, buf.get(), bufSize);
                    }

                    break;
                }

                Logger::warn("Title Hash Container", "Error while reading file {}", info.path);
                Logger::warn("Title Hash Container", file->lastResult());

                m_invalidHash = true;
                return;
            }

            offset += bytesRead;
            md5Update(&ctx, buf.get(), bytesRead);
        }

        md5Finalize(&ctx);

        char hash[(sizeof(MD5Context::digest) * 2) + 1];
        hash[sizeof(hash) - 1] = 0;

        for(u8 i = 0; i < sizeof(MD5Context::digest); i++) {
            sprintf(&hash[i * 2], "%02x", ctx.digest[i]);
        }

        info._shouldUpdateHash = false;
        info.hash              = hash;

        it++;
    }

    updateTotalSizes();

    const u64 stop = svcGetSystemTick();
    Logger::info("Title Hash Container", "Took {:02f} seconds", ((stop - start) / 268) / 1e+6);

    m_invalidHash = false;

    lock.release();
    saveCache();
}

constexpr std::string formatFileInfo(Container container, FileInfo file) {
    return std::format("{}{}{}:{}\n", container == SAVE ? 's' : 'e', file.size, file.path, file.hash.value_or(""));
}

constexpr size_t versionSize = 3;

bool Title::loadSMDHData() {
    if(!m_valid) return false;
    Logger::info("Title", "Loading SMDH data for: {:016X}", m_id);

    std::unique_ptr<SMDH> smdh = std::make_unique<SMDH>(m_id, m_mediaType);
    if(!smdh->valid()) {
        Logger::warn("Title", "No SMDH data for: {:016X}", m_id);
        return false;
    }

    std::string longDescription = StringUtil::toUTF8(smdh->applicationTitle(1).longDescription);
    std::replace(longDescription.begin(), longDescription.end(), '\n', ' ');

    strncpy(m_longDescription, longDescription.c_str(), sizeof(m_longDescription) - 1);

    m_tex  = smdh->bigTex();
    m_icon = { m_tex->handle(), &SMDH::ICON_SUBTEX };

    return true;
}

void Title::saveCache(bool lockCache, bool lockContainer) {
    if(!m_valid || m_mediaType != MEDIATYPE_SD) return;
    PROFILE_SCOPE("Save Title Cache");

    // deffered is inverse to should lock
    ScopedLock cacheLock(m_cacheMutex, !lockCache);

    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    if(sdmc == nullptr || !sdmc->valid()) {
        Logger::error("Save Title Cache", "Failed to open sdmc");
        return;
    }

    if(!sdmc->mkdir(DATA_DIRECTORY_U, 0, true)) {
        Logger::error("Save Title Cache", "Failed to create data directory");
        return;
    }

    std::string path = std::format(DATA_DIRECTORY "/{:04X}", uniqueID());
    sdmc->deleteFile(path);

    if((m_icon.tex == nullptr || m_tex == nullptr) && !loadSMDHData()) {
        Logger::error("Save Title Cache", "Failed to load SMDH data");
        return;
    }

    std::ostringstream stream;
    // version
    stream << TITLE_CACHE_VER;
    stream << std::left << std::setfill('\0') << std::setw(sizeof(SMDH::ApplicationTitle::longDescription) / sizeof(u16)) << m_longDescription;

    // copy image data directly to stream
    const u16* src = reinterpret_cast<u16*>(m_icon.tex->data) + (SMDH::ICON_DATA_WIDTH - SMDH::ICON_WIDTH) * SMDH::ICON_DATA_HEIGHT;
    for(u16 j = 0; j < SMDH::ICON_HEIGHT; j += 8) {
        stream.write(reinterpret_cast<const char*>(src), SMDH::ICON_WIDTH * 8 * sizeof(u16));
        src += SMDH::ICON_DATA_WIDTH * 8;
    }

    for(auto container : { SAVE, EXTDATA }) {
        ScopedLock containerLock(containerMutex(container), !lockContainer);

        for(auto file : getContainerFiles(container)) {
            stream << formatFileInfo(container, file);
        }
    }

    PROFILE_SCOPE("Save Cache File");
    auto file = sdmc->openFile(path, FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if(file == nullptr || !file->valid()) {
        Logger::warn("Save Title Cache", "Failed to open cache file");
        return;
    }

    const std::string& str = stream.str();
    if(!file->setSize(str.size())) {
        Logger::warn("Save Title Cache", "Failed to set cache file size");
        Logger::warn("Save Title Cache", file->lastResult());

        return;
    }

    u32 wrote = file->write(str.c_str(), str.size(), 0, FS_WRITE_FLUSH);
    if(wrote == 0 || wrote == UINT32_MAX) {
        Logger::warn("Save Title Cache", "Failed to write cache data");
        Logger::warn("Save Title Cache", file->lastResult());
    }
}

bool Title::loadCache() {
    if(!m_valid) return false;

    // TODO: should use cached smdh, cant rely on cached container files as it could be a different cart
    if(m_mediaType != MEDIATYPE_SD) {
        if(m_saveAccessible) loadContainerFiles(SAVE);
        if(m_extdataAccessible) loadContainerFiles(EXTDATA);

        return loadSMDHData();
    }

    struct TitleData {
        char longDesc[sizeof(SMDH::ApplicationTitle::longDescription) / sizeof(u16)];
        u16 texData[SMDH::ICON_WIDTH * SMDH::ICON_HEIGHT];
    };

    PROFILE_SCOPE("Load Title Cache");

    auto lock        = m_cacheMutex.lock();
    auto saveLock    = m_saveMutex.lock();
    auto extdataLock = m_extdataMutex.lock();

    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    std::shared_ptr<File> file;

    if(sdmc == nullptr || !sdmc->valid()) {
        goto invalidSDMC;
    }

    file = sdmc->openFile(std::format(DATA_DIRECTORY "/{:04X}", uniqueID()), FS_OPEN_READ, 0);
    if(file == nullptr || !file->valid()) {
        Logger::info("Load Cached Title Files", "Cache doesn't exist for {:016X}, creating", m_id);

        if(false) {
        invalidSDMC:
            Logger::info("Load Title Cached Files", "Failed to open sdmc");
            if(sdmc != nullptr) {
                Logger::info("Load Title Cached Files", sdmc->lastResult());
            }
        }

        if(false) {
        invalidCache:
            Logger::info("Load Title Cached Files", "Cache doesn't have required entries for {:016X}, updating", m_id);
        }

    invalid:
        m_saveFiles.clear();
        m_extdataFiles.clear();

        m_icon = { nullptr, nullptr };
        m_tex.reset();

        file.reset();

        if(m_saveAccessible) loadContainerFiles(SAVE, false, nullptr, false);
        if(m_extdataAccessible) loadContainerFiles(EXTDATA, false, nullptr, false);

        saveCache(false, false);
        return m_icon.tex != nullptr;
    }

    const u64 fileSize = file->size();
    if(R_FAILED(file->lastResult()) || fileSize == U64_MAX) {
        goto invalidCache;
    }

    char version[versionSize];

    u64 read = file->read(version, versionSize, 0);
    if(read != versionSize || R_FAILED(file->lastResult())) {
        goto invalidCache;
    }

    if(strncmp(version, TITLE_CACHE_VER, versionSize) != 0) {
        goto invalidCache;
    }

    u64 fileOffset = read;
    if(fileSize - fileOffset < sizeof(TitleData)) {
        goto invalidCache;
    }

    std::unique_ptr<TitleData> titleData;
    titleData.reset(new TitleData);

    read = file->read(titleData.get(), sizeof(TitleData), fileOffset);
    if(read != sizeof(TitleData) || R_FAILED(file->lastResult())) {
        goto invalidCache;
    }

    fileOffset += read;
    strncpy(m_longDescription, titleData->longDesc, sizeof(m_longDescription) - 1);

    m_tex  = TexWrapper::create(SMDH::ICON_DATA_WIDTH, SMDH::ICON_DATA_HEIGHT, GPU_RGB565);
    m_icon = { m_tex->handle(), &SMDH::ICON_SUBTEX };

    SMDH::copyImageData(titleData->texData, SMDH::ICON_WIDTH, SMDH::ICON_HEIGHT, reinterpret_cast<u16*>(m_icon.tex->data), SMDH::ICON_DATA_WIDTH, SMDH::ICON_DATA_HEIGHT);

    m_saveFiles.clear();
    m_extdataFiles.clear();

    constexpr u64 maxBuf = 0x1000;
    u64 bufSize          = 0;
    char buf[maxBuf];

    enum ReadingType {
        CONTAINER_CODE = 0,
        SIZE,
        PATH,
        HASH
    };

    ReadingType reading = CONTAINER_CODE;
    u64 offset          = bufSize;

    char container = '\0';
    u64 size       = 0;

    constexpr u32 pathMax = 0x100;
    u32 pathSize          = 0;
    char path[pathMax];

    constexpr u32 hashMax = 32;
    u32 hashSize          = 0;
    char hash[hashMax];

    while(true) {
        if(offset >= bufSize) {
            offset = 0;

            bufSize = file->read(buf, maxBuf, fileOffset);
            if(bufSize == U64_MAX || R_FAILED(file->lastResult())) {
                Logger::error("Load Title Cached Files", "Failed to read cache file");
                goto invalid;
            }
            else if(bufSize <= 0) {
                break;
            }

            fileOffset += bufSize;
        }

        switch(reading) {
        case CONTAINER_CODE:
            container = buf[offset];
            offset++;

            reading = SIZE;
            break;
        case SIZE:
            for(; offset < bufSize; offset++) {
                if(!isdigit(static_cast<int>(buf[offset]))) {
                    reading = PATH;
                    break;
                }

                size *= 10;
                size += static_cast<u64>(buf[offset] - '0');
            }

            break;
        case PATH:
            for(; offset < bufSize; offset++) {
                char c = buf[offset];
                if(c == ':') {
                    reading = HASH;
                    offset++;

                    break;
                }
                else if(pathSize + 1 >= pathMax) {
                    goto invalidCache;
                }

                path[pathSize++] = c;
            }

            break;
        case HASH:
            for(; offset < bufSize; offset++) {
                char c = buf[offset];
                if(c == '\n' || c == '\0') {
                    goto addEntry;
                }
                else if(hashSize + 1 > hashMax) {
                    goto invalidCache;
                }

                hash[hashSize++] = c;
            }

            break;
        default: goto invalidCache;
        }

        continue;
    addEntry:
        if(hashSize != 0 && hashSize != hashMax) {
            goto invalidCache;
        }

        reading = CONTAINER_CODE;

        std::string pathStr = std::string(path, pathSize);
        std::optional<std::string> hashStr;
        if(hashSize != 0) {
            hashStr = std::string(hash, hashSize);
        }

        FileInfo info = {
            .path = pathStr,
            .hash = hashStr,
            .size = size,

            ._shouldUpdateHash = true,
        };

        switch(container) {
        case 's': m_saveFiles.push_back(info); break;
        case 'e': m_extdataFiles.push_back(info); break;
        default:  goto invalidCache;
        }

        container = '\0';
        size      = 0;
        pathSize  = 0;
        hashSize  = 0;
        offset++;
    }

    updateTotalSizes();

    return true;
}

bool Title::isOutOfSync() const {
    if(!m_valid) return false;
    return m_outOfSync != 0;
}

u8 Title::outOfSync() const {
    if(!m_valid) return false;
    return m_outOfSync;
}

void Title::setOutOfSync(u8 outOfSync) {
    if(!m_valid) return;
    m_outOfSync = outOfSync;
}