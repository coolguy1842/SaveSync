#include <Debug/Logger.hpp>
#include <Debug/Profiler.hpp>
#include <TitleLoader.hpp>
#include <map>

TitleLoader::TitleLoader()
    : m_SDTitlesLoaded(false)
    , m_loaderWorker(std::make_unique<Worker>([this](Worker*) { loadWorkerMain(); }, 4, 0x9000))
    , m_hashWorker(std::make_unique<Worker>([this](Worker*) { hashWorkerMain(); }, 3, 0x3000)) {
    amInit();
    reloadTitles();
}

TitleLoader::~TitleLoader() {
    titlesLoadedChangedSignal.clear();
    titlesFinishedLoadingSignal.clear();
    titleHashedSignal.clear();

    m_loaderWorker->waitForExit();
    m_loaderWorker.reset();

    m_hashWorker->waitForExit();
    m_hashWorker.reset();

    amExit();
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

void TitleLoader::loadSDTitles(u32 numTitles) {
    PROFILE_SCOPE("Load SD Titles");

    Result res;
    if(numTitles == 0) {
        if(R_FAILED(res = AM_GetTitleCount(MEDIATYPE_SD, &numTitles))) {
            Logger::warn("Load SD Titles", "Failed to get title count");
            Logger::warn("Load SD Titles", res);
            return;
        }
    }

    if(numTitles == 0) {
        Logger::info("Load SD Titles", "Title count is 0");
        return;
    }

    std::vector<u64> ids(numTitles);
    if(R_FAILED(res = AM_GetTitleList(&numTitles, MEDIATYPE_SD, numTitles, ids.data()))) {
        Logger::warn("Load SD Titles", "Failed to get SDCard title list");
        Logger::warn("Load SD Titles", res);

        return;
    }

    for(u64 id : ids) {
        // Logger::log("Load SD Titles", "Loading {:X}", id);

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

void TitleLoader::loadWorkerMain() {
    Logger::info("Load Worker", "Loading titles");
    PROFILE_SCOPE("Load All Titles");

    // TODO: maybe implement game cartridge loading
    for(auto it = m_titles.begin(); it != m_titles.end();) {
        std::shared_ptr<Title> title = *it;
        if(title->mediaType() != MEDIATYPE_SD) {
            it = m_titles.erase(it);
            continue;
        }

        it++;
    }

    u32 sdTitles  = 0;
    m_totalTitles = 0;

    if(!m_SDTitlesLoaded) {
        Result res;
        if(R_SUCCEEDED(res = AM_GetTitleCount(MEDIATYPE_SD, &sdTitles))) {
            m_totalTitles += sdTitles;
        }
        else {
            Logger::warn("Load Worker", "Failed to get SDCard title count");
            Logger::warn("Load Worker", res);

            sdTitles = 0;
        }

        m_titlesLoaded = 0;
        titlesLoadedChangedSignal(m_titlesLoaded);

        loadSDTitles(sdTitles);
        m_SDTitlesLoaded = true;
    }
    else {
        sdTitles = m_totalTitles = m_titlesLoaded = m_titles.size();
        titlesLoadedChangedSignal(m_titlesLoaded);
    }

    if(m_loaderWorker->waitingForExit()) {
        Logger::info("Load Worker", "Exiting early");
        return;
    }

    Logger::info("Load Worker", "Loaded {} titles", m_titles.size());
    titlesFinishedLoadingSignal();
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

    std::vector<std::shared_ptr<Title>> titles = m_titles;
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

    for(const auto& entry : containers) {
        for(const auto& pair : entry.second) {
            pair.first->hashContainer(pair.second);

            if(m_hashWorker->waitingForExit()) {
                Logger::info("Hash Worker", "Exiting early");
                return;
            }

            titleHashedSignal(pair.first, pair.second);
        }
    }

    Logger::info("Hash Worker", "Hashed all titles");
}

std::vector<std::shared_ptr<Title>> TitleLoader::titles() { return m_titles; }
