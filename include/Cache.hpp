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
    std::shared_ptr<File> m_cacheFile;
    std::shared_ptr<File> m_mapFile;
};

#endif