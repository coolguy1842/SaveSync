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
    , m_id(id)
    , m_mediaType(mediaType)
    , m_cardType(cardType)
    , m_outOfDate(0) {
    PROFILE_SCOPE("Load Title");

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

    if(!loadCache()) {
        return;
    }

    m_valid = true;
}

Title::~Title() {
    if(m_icon.tex != nullptr) {
        C3D_TexDelete(m_icon.tex);
        delete m_icon.tex;
    }
}

bool Title::valid() const { return m_valid; }
FS_MediaType Title::mediaType() const { return m_mediaType; }
FS_CardType Title::cardType() const { return m_cardType; }

u64 Title::id() const { return m_id; }
u32 Title::highID() const { return static_cast<u32>(m_id >> 32); }
u32 Title::lowID() const { return static_cast<u32>(m_id); }
u32 Title::uniqueID() const { return (lowID() >> 8); }

const char* Title::productCode() const { return m_productCode; }

std::string Title::shortDescription() const { return m_shortDescription; }
std::string Title::longDescription() const { return m_longDescription; }
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
    switch(container) {
    case SAVE:    return m_saveAccessible;
    case EXTDATA: return m_extdataAccessible;
    default:      return false;
    }
}

Result Title::deleteSecureSaveValue() {
    if(m_cardType != CARD_CTR) {
        Logger::warn("Title", "Can't delete secure save value for non CTR title: {:X}", m_id);
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_NOT_FOUND);
    }

    u8 out;
    u64 secureValue = (static_cast<u64>(SECUREVALUE_SLOT_SD) << 32) | (uniqueID() << 8);
    return FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &secureValue, 8, &out, 1);
}

void Title::reloadContainerFiles(Container container) { loadContainerFiles(container); }
void Title::resetContainerFiles(Container container) {
    switch(container) {
    case SAVE:    m_saveFiles.clear(); break;
    case EXTDATA: m_extdataFiles.clear(); break;
    default:      return;
    }

    reloadContainerFiles(container);
}

std::vector<FileInfo> Title::getContainerFiles(Container container) const {
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

void Title::setContainerFiles(std::vector<FileInfo>& files, Container container) {
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

    saveCache();
}

void Title::loadContainerFiles(Container container, bool cache, std::shared_ptr<Archive> archive, bool shouldLock) {
    ScopedLock lock = ScopedLock(containerMutex(container), true);
    if(shouldLock) {
        lock.lock();
    }

    if(archive == nullptr) {
        archive = openContainer(container);
    }

    if(archive == nullptr || !archive->valid()) {
        Logger::warn("Load Title Container", "Archive invalid, title: {:X} container: {}", m_id, getContainerName(container));
        return;
    }

    PROFILE_SCOPE("Load Title Container");
    std::vector<FileInfo>& files = containerFiles(container);
    std::unordered_map<std::u16string, FileInfo> oldFiles;

    for(const auto& file : files) {
        oldFiles.emplace(file.nativePath, file);
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

            auto it = oldFiles.find(entry->path());
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
                .nativePath = entry->path(),
                .path       = StringUtil::toUTF8(entry->path()),

                .size = size,

                ._shouldUpdateHash = true,
            });
        }

    skip:
        directories.pop_front();
    }

    std::sort(newFiles.begin(), newFiles.end());
    files.swap(newFiles);

    if(cache) {
        lock.release();
        saveCache();
    }
}

void Title::hashContainer(Container container) {
    auto lock = containerMutex(container).lock();

    std::shared_ptr<Archive> archive = openContainer(container);
    loadContainerFiles(container, false, archive, false);

    std::vector<FileInfo>& files = containerFiles(container);
    if(archive == nullptr || !archive->valid()) {
        return;
    }

    PROFILE_SCOPE("Hash Container");

    u64 newSize;
    for(auto it = files.begin(); it != files.end();) {
        FileInfo& info = *it;

        std::shared_ptr<File> file = archive->openFile(info.nativePath, FS_OPEN_READ, 0);
        if(file == nullptr || !file->valid() || (newSize = file->size()) == U64_MAX) {
            it = files.erase(it);
            continue;
        }

        info.size = file->size();

        MD5Context ctx;
        md5Init(&ctx);

        u64 offset = 0;
        std::vector<u8> buf;

        do {
            buf = file->read(0x1000, offset);
            offset += buf.size();

            md5Update(&ctx, buf.data(), buf.size());
        } while(buf.size() >= 1);

        md5Finalize(&ctx);

        char hash[33];
        for(u8 i = 0; i < 16; i++) {
            snprintf(&hash[i * 2], 3, "%02x", ctx.digest[i]);
        }

        info._shouldUpdateHash = false;
        info.hash              = hash;

        it++;
    }

    saveCache();
}

constexpr std::string formatFileInfo(Container container, FileInfo file) {
    return std::format("{}{}{}:{}\n", container == SAVE ? 's' : 'e', file.size, file.path, file.hash.value_or(" "));
}
constexpr size_t versionSize = 3;

bool Title::loadSMDHData() {
    std::unique_ptr<SMDH> smdh = std::make_unique<SMDH>(m_id, m_mediaType);
    if(!smdh->valid()) {
        Logger::warn("Title", "No SMDH data for: {:X}", m_id);
        return false;
    }

    m_shortDescription = StringUtil::toUTF8(smdh->applicationTitle(1).shortDescription);
    std::replace(m_shortDescription.begin(), m_shortDescription.end(), '\n', ' ');

    m_longDescription = StringUtil::toUTF8(smdh->applicationTitle(1).longDescription);
    std::replace(m_longDescription.begin(), m_longDescription.end(), '\n', ' ');

    m_icon = smdh->bigIcon();
    return true;
}

void Title::saveCache() {
    PROFILE_SCOPE("Save Title Cache");

    auto lock                     = m_cacheMutex.lock();
    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    if(sdmc == nullptr || !sdmc->valid()) {
        Logger::error("Save Title Cache", "Failed to open sdmc");
        return;
    }

    if(!sdmc->mkdir(u"/3ds/SaveSync", 0, true)) {
        Logger::error("Save Title Cache", "Failed to create SaveSync directory");
        return;
    }

    std::string path = std::format("/3ds/SaveSync/{:X}", m_id);
    sdmc->deleteFile(path);

    if(m_icon.tex == nullptr && !loadSMDHData()) {
        Logger::error("Save Title Cache", "Failed to load SMDH data");
        return;
    }

    std::ostringstream stream;
    // version
    stream << std::setfill('0') << std::setw(versionSize) << "1";
    stream << std::left << std::setfill('\0') << std::setw(sizeof(SMDH::ApplicationTitle::shortDescription) / sizeof(u16)) << m_shortDescription;
    stream << std::left << std::setfill('\0') << std::setw(sizeof(SMDH::ApplicationTitle::longDescription) / sizeof(u16)) << m_longDescription;

    // copy image data directly to stream
    const u16* src = reinterpret_cast<u16*>(m_icon.tex->data) + (SMDH::ICON_DATA_WIDTH - SMDH::ICON_WIDTH) * SMDH::ICON_DATA_HEIGHT;
    for(u16 j = 0; j < SMDH::ICON_HEIGHT; j += 8) {
        stream.write(reinterpret_cast<const char*>(src), SMDH::ICON_WIDTH * 8 * sizeof(u16));
        src += SMDH::ICON_DATA_WIDTH * 8;
    }

    for(auto container : { SAVE, EXTDATA }) {
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
    struct TitleData {
        char shortDesc[sizeof(SMDH::ApplicationTitle::shortDescription) / sizeof(u16)], longDesc[sizeof(SMDH::ApplicationTitle::longDescription) / sizeof(u16)];
        u16 texData[SMDH::ICON_WIDTH * SMDH::ICON_HEIGHT];
    };

    PROFILE_SCOPE("Load Title Cache");

    auto lock                     = m_cacheMutex.lock();
    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    std::shared_ptr<File> file;

    if(sdmc == nullptr || !sdmc->valid()) {
        goto invalidSDMC;
    }

    file = sdmc->openFile(std::format("/3ds/SaveSync/{:X}", m_id), FS_OPEN_READ, 0);
    if(file == nullptr || !file->valid()) {
        Logger::info("Load Cached Title Files", "Cache doesn't exist for {:X}, creating", m_id);

        if(false) {
        invalidSDMC:
            Logger::info("Load Title Cached Files", "Failed to open sdmc");
            if(sdmc != nullptr) {
                Logger::info("Load Title Cached Files", sdmc->lastResult());
            }
        }

        if(false) {
        invalidCache:
            Logger::info("Load Title Cached Files", "Cache doesn't have required entries for {:X}, updating", m_id);
        }

        if(m_icon.tex != nullptr) {
            delete m_icon.tex;
            m_icon = { nullptr, nullptr };
        }

        file.reset();
        lock.release();

        if(m_saveAccessible) loadContainerFiles(SAVE, false);
        if(m_extdataAccessible) loadContainerFiles(EXTDATA, false);

        saveCache();
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

    if(strncmp(version, "001", versionSize) != 0) {
        goto invalidCache;
    }

    size_t offset = read;
    if(fileSize - offset < sizeof(TitleData)) {
        goto invalidCache;
    }

    TitleData titleData;
    read = file->read(&titleData, sizeof(TitleData), offset);

    if(read != sizeof(titleData) || R_FAILED(file->lastResult())) {
        goto invalidCache;
    }

    offset += read;
    m_shortDescription = titleData.shortDesc;
    m_longDescription  = titleData.longDesc;

    m_icon = { new C3D_Tex, &SMDH::ICON_SUBTEX };
    C3D_TexInit(m_icon.tex, SMDH::ICON_DATA_WIDTH, SMDH::ICON_DATA_HEIGHT, GPU_RGB565);
    SMDH::copyImageData(titleData.texData, SMDH::ICON_WIDTH, SMDH::ICON_HEIGHT, reinterpret_cast<u16*>(m_icon.tex->data), SMDH::ICON_DATA_WIDTH, SMDH::ICON_DATA_HEIGHT);

    m_saveFiles.clear();
    m_extdataFiles.clear();

    std::string fileBuf = file->readStr(fileSize - offset, offset);
    offset              = 0;

    while(offset < fileBuf.size()) {
        char containerCode = fileBuf[offset];
        if((containerCode != 's' && containerCode != 'e')) {
            break;
        }

        FileInfo info = { ._shouldUpdateHash = true };
        char path[0x100];

        char hash[33];
        int numRead = 0;
        if(sscanf(fileBuf.c_str() + offset, "%*c%llu%[^:]:%32[^\n]\n%n", &info.size, path, hash, &numRead) != 3) {
            Logger::warn("Load Title Cache", "Invalid hashed entry");
            goto invalidCache;
        }

        if(strlen(hash) == 32) {
            info.hash = hash;
        }

        info.path = path;
        offset += static_cast<u64>(numRead);

        switch(containerCode) {
        case 's': m_saveFiles.push_back(info); break;
        case 'e': m_extdataFiles.push_back(info); break;
        default:  break;
        }
    }

    return true;
}

u8 Title::outOfDate() const { return m_outOfDate; }
void Title::setOutOfDate(u8 outOfDate) { m_outOfDate = outOfDate; }