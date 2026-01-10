#include <Cache.hpp>
#include <FS/Archive.hpp>

// TODO: need to work out how cache will function, mostly a stub for now
Cache::Cache()
    : m_valid(false) {
    std::shared_ptr<File> cacheFile = openCacheFile();
    if(cacheFile == nullptr || !cacheFile->valid()) {
        return;
    }

    std::shared_ptr<File> mapFile = openMapFile();
    if(mapFile == nullptr || !mapFile->valid()) {
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

std::shared_ptr<File> Cache::openCacheFile(u32 flags) { return openFile(DATA_DIRECTORY_U u"/cache", flags); }
std::shared_ptr<File> Cache::openMapFile(u32 flags) { return openFile(DATA_DIRECTORY_U u"/cache.map", flags); }
std::shared_ptr<File> Cache::openFile(VarPath path, u32 flags) {
    std::shared_ptr<Archive> sdmc = Archive::sdmc();
    if(sdmc == nullptr || !sdmc->valid() || !sdmc->mkdir(DATA_DIRECTORY_U, 0, true)) {
        return nullptr;
    }

    return sdmc->openFile(path, flags, 0);
}