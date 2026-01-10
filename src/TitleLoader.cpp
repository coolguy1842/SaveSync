#include <Debug/Logger.hpp>
#include <Debug/Profiler.hpp>
#include <TitleLoader.hpp>
#include <Util/EmuUtil.hpp>
#include <Util/StringUtil.hpp>
#include <map>

// hopefully enough
constexpr size_t maxHashBufSize = 0x10000;
TitleLoader::TitleLoader()
    : m_lastCardID(0)
    , m_cardWorker(std::make_unique<Worker>([this](Worker*) { cardWorkerMain(); }, 2, 0x1000, Worker::SYSCORE))
    , m_loaderWorker(std::make_unique<Worker>([this](Worker*) { loadWorkerMain(); }, 4, maxHashBufSize + 0x6000, Worker::APPCORE))
    , m_hashWorker(std::make_unique<Worker>([this](Worker*) { hashWorkerMain(); }, 3, maxHashBufSize + 0x2000, Worker::APPCORE)) {
    // needed for SYSCORE thread
    APT_SetAppCpuTimeLimit(5);
    reloadTitles();
}

TitleLoader::~TitleLoader() {
    titlesLoadedChangedSignal.clear();
    titlesFinishedLoadingSignal.clear();
    titleHashedSignal.clear();

    m_cardWorker->signalShouldExit();
    m_condVar.broadcast();

    m_loaderWorker->signalShouldExit();
    m_hashWorker->signalShouldExit();

    m_loaderWorker->waitForExit();
    m_hashWorker->waitForExit();
    m_cardWorker->waitForExit();
}

void TitleLoader::reloadTitles() {
    m_loaderWorker->waitForExit();
    m_loaderWorker->start();
}

void TitleLoader::reloadHashes() {
    m_hashWorker->waitForExit();
    m_hashWorker->start();
}

size_t TitleLoader::totalTitles() const { return m_totalTitles; }
size_t TitleLoader::titlesLoaded() const { return m_titlesLoaded; }

bool TitleLoader::isLoadingTitles() const { return m_loaderWorker->running(); }
bool TitleLoader::loadGameCardTitle() {
    if(EmulatorUtil::isEmulated()) {
        return false;
    }

    auto cleanupLastCard = [this]() {
        if(m_lastCardID == 0) {
            return;
        }

        auto lock = m_titlesMutex.lock();
        for(auto it = m_titles.begin(); it != m_titles.end();) {
            if(*it == nullptr) {
                it = m_titles.erase(it);

                m_totalTitles--;
                titlesLoadedChangedSignal(--m_titlesLoaded);

                continue;
            }

            std::shared_ptr<Title> title = *it;
            if(title->id() == m_lastCardID && title->mediaType() == MEDIATYPE_GAME_CARD) {
                title->setInvalid();
                title = nullptr;

                it = m_titles.erase(it);

                m_totalTitles--;
                titlesLoadedChangedSignal(--m_titlesLoaded);

                break;
            }

            it++;
        }

        m_lastCardID = 0;
    };

    FS_CardType type;
    u32 count;
    u64 id;
    if(
        R_FAILED(FSUSER_GetCardType(&type)) || type != CARD_CTR ||
        R_FAILED(AM_GetTitleCount(MEDIATYPE_GAME_CARD, &count)) || count != 1 ||
        R_FAILED(AM_GetTitleList(NULL, MEDIATYPE_GAME_CARD, 1, &id))
    ) {
        cleanupLastCard();
        return false;
    }
    else if(id == m_lastCardID) {
        return false;
    }

    cleanupLastCard();

    m_lastCardID = id;
    Logger::info("Card Worker", "Loading {:X}", id);

    std::shared_ptr<Title> title = std::make_shared<Title>(id, MEDIATYPE_GAME_CARD, CARD_CTR);
    if(title == nullptr || !title->valid()) {
        Logger::info("Card Worker", "{:X} Invalid", id);
        return false;
    }

    {
        auto lock = m_titlesMutex.lock();
        m_titles.insert(m_titles.begin(), title);
    }

    static bool first = true;
    if(!first) {
        m_totalTitles++;
    }

    first = false;
    titlesLoadedChangedSignal(++m_titlesLoaded);

    return true;
}

void TitleLoader::loadSDTitles(u32 numTitles) {
    PROFILE_SCOPE("Load SD Titles");

    Result res;
    if(numTitles == 0) {
        return;
    }

    if(numTitles == UINT32_MAX) {
        if(R_FAILED(res = AM_GetTitleCount(MEDIATYPE_SD, &numTitles))) {
            Logger::warn("Load SD Titles", "Failed to get title count");
            Logger::warn("Load SD Titles", res);
            return;
        }
    }

    std::vector<u64> ids(numTitles);
    if(R_FAILED(res = AM_GetTitleList(&numTitles, MEDIATYPE_SD, ids.size(), ids.data()))) {
        Logger::warn("Load SD Titles", "Failed to get SDCard title list");
        Logger::warn("Load SD Titles", res);

        return;
    }

    for(u64 id : ids) {
        Logger::info("Load SD Titles", "Loading {:X}", id);

        if(m_loaderWorker->waitingForExit()) {
            Logger::info("Load SD Titles", "Exiting early");
            return;
        }

        std::shared_ptr<Title> title = std::make_shared<Title>(id, MEDIATYPE_SD, CARD_CTR);
        if(title != nullptr && title->valid()) {
            auto lock = m_titlesMutex.lock();
            m_titles.push_back(title);
        }

        titlesLoadedChangedSignal(++m_titlesLoaded);
    }
}

void TitleLoader::cardWorkerMain() {
    Logger::info("Card Worker", "Starting watcher");
    constexpr s64 checkIntervalMS = 100;

    do {
        if(loadGameCardTitle()) {
            reloadHashes();
        }
    } while(m_condVar.wait(checkIntervalMS * static_cast<s64>(1e+6)) != 0 && !m_cardWorker->waitingForExit());
}

void TitleLoader::loadWorkerMain() {
    Logger::info("Load Worker", "Loading titles");
    PROFILE_SCOPE("Load All Titles");

    m_cardWorker->waitForExit();

    {
        auto lock = m_titlesMutex.lock();
        m_titles.clear();
    }

    m_titlesLoaded = 0;
    m_totalTitles  = 0;

    u32 cardTitles = 0;
    u32 sdTitles   = 0;

    Result res;
    if(R_SUCCEEDED(res = AM_GetTitleCount(MEDIATYPE_GAME_CARD, &cardTitles))) {
        m_totalTitles += cardTitles;
    }
    else {
        Logger::warn("Load Worker", "Failed to get Game Card title count");
        Logger::warn("Load Worker", res);

        cardTitles = 0;
    }

    if(R_SUCCEEDED(res = AM_GetTitleCount(MEDIATYPE_SD, &sdTitles))) {
        m_totalTitles += sdTitles;
    }
    else {
        Logger::warn("Load Worker", "Failed to get SD Card title count");
        Logger::warn("Load Worker", res);

        sdTitles = 0;
    }

    titlesLoadedChangedSignal(m_titlesLoaded);

    loadGameCardTitle();
    loadSDTitles(sdTitles);

    if(m_loaderWorker->waitingForExit()) {
        Logger::info("Load Worker", "Exiting early");
        return;
    }

    Logger::info("Load Worker", "Loaded {} titles", m_titles.size());
    titlesFinishedLoadingSignal();
    if(!EmulatorUtil::isEmulated()) {
        m_cardWorker->start();
    }

    reloadHashes();
}

enum Priority {
    HIGH,
    MEDIUM,
    LOW
};

void TitleLoader::hashWorkerMain() {
    Logger::info("Hash Worker", "Hashing all titles");

    PROFILE_SCOPE("Hash All Titles");
    std::map<Priority, std::vector<std::pair<std::shared_ptr<Title>, Container>>> containers = {
        { HIGH, {} },
        { MEDIUM, {} },
        { LOW, {} }
    };

    std::vector<std::shared_ptr<Title>> titles;
    {
        auto lock = m_titlesMutex.lock();
        titles    = m_titles;
    }

    for(auto title : titles) {
        if(title == nullptr || !title->valid()) {
            continue;
        }

        for(Container type : { SAVE, EXTDATA }) {
            if(m_hashWorker->waitingForExit()) {
                Logger::info("Hash Worker", "Exiting early");
                return;
            }

            std::vector<FileInfo> files;
            {
                // this will wait for any operation to be done
                auto lock = title->containerMutex(type).lock();
                files     = title->getContainerFiles(type);
            }

            if(files.size() <= 0) {
                continue;
            }

            bool hasAnyHash = false, allHashed = true;
            for(auto file : files) {
                if(!file.hash.has_value()) {
                    allHashed = false;
                    break;
                }
                // make medium priority if should update
                else if(file._shouldUpdateHash) {
                    allHashed  = false;
                    hasAnyHash = true;
                    break;
                }

                hasAnyHash = true;
            }

            containers[hasAnyHash ? (allHashed ? LOW : MEDIUM) : HIGH].push_back({ title, type });
        }
    }

    // must do 0x1000 bytes at a time (1 block), or will miss out on non invalid data
    constexpr size_t bufSize = 0x1000;

    std::shared_ptr<u8> hashBuf;
    hashBuf.reset(reinterpret_cast<u8*>(malloc(bufSize)));

    for(const auto& entry : containers) {
        for(const auto& pair : entry.second) {
            pair.first->hashContainer(pair.second, hashBuf, bufSize, m_hashWorker.get());

            if(m_hashWorker->waitingForExit()) {
                Logger::info("Hash Worker", "Exiting early");
                return;
            }

            titleHashedSignal(pair.first, pair.second);
        }
    }

    Logger::info("Hash Worker", "Hashed all titles");
}

std::vector<std::shared_ptr<Title>> TitleLoader::titles() {
    auto lock = m_titlesMutex.lock();
    return m_titles;
}