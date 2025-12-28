#include <Cache.hpp>
#include <FS/Archive.hpp>

// TODO: need to work out how cache will function, mostly a stub for now
Cache::Cache()
    : m_valid(false) {
    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    if(sdmc == nullptr || !sdmc->valid() || !sdmc->mkdir(u"/3ds/" EXE_NAME, 0, true)) {
        return;
    }

    m_cacheFile = sdmc->openFile(u"/3ds/" EXE_NAME "/cache", FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if(m_cacheFile == nullptr || !m_cacheFile->valid()) {
        return;
    }

    m_mapFile = sdmc->openFile(u"/3ds/" EXE_NAME "/cache.map", FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    if(m_mapFile == nullptr || !m_mapFile->valid()) {
        return;
    }

    m_valid = true;
}

Cache::~Cache() {
}

bool Cache::valid() const { return m_valid; }

void Cache::updateTitle(std::shared_ptr<Title> title) {
    (void)title;
}