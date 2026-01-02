#ifndef __CACHE_HPP__
#define __CACHE_HPP__

#include <FS/File.hpp>
#include <Title.hpp>
#include <memory>
#include <unordered_map>

class Cache {
public:
    Cache();
    ~Cache();

    bool valid() const;
    void updateTitle(std::shared_ptr<Title> title);

private:
    bool m_valid;

    std::shared_ptr<File> openFile(VarPath path, u32 flags = FS_OPEN_CREATE | FS_OPEN_READ);

    std::shared_ptr<File> openCacheFile(u32 flags = FS_OPEN_CREATE | FS_OPEN_READ);
    std::shared_ptr<File> openMapFile(u32 flags = FS_OPEN_CREATE | FS_OPEN_READ);
};

#endif