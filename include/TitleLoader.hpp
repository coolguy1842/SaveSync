#ifndef __TITLE_LOADER_HPP__
#define __TITLE_LOADER_HPP__

#include <3ds.h>

#include <Title.hpp>
#include <array>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include <Util/Worker.hpp>

class TitleLoader {
public:
    TitleLoader();
    ~TitleLoader();

    void reloadTitles();
    void reloadHashes();

    size_t totalTitles() const;
    size_t titlesLoaded() const;

    bool isLoadingTitles() const;

    std::vector<std::shared_ptr<Title>> titles();

private:
    void loadSDTitles(u32 numTitles = 0);

    void hashWorkerMain();
    void loadWorkerMain();

private:
    std::mutex m_titlesMutex;
    std::vector<std::shared_ptr<Title>> m_titles;

    struct TitleEntry {
        u64 id;
        FS_MediaType mediaType;
        FS_CardType cardType;
    };

    bool m_SDTitlesLoaded;

    size_t m_totalTitles               = 0;
    std::atomic<size_t> m_titlesLoaded = 0;

    std::unique_ptr<Worker> m_loaderWorker;
    std::unique_ptr<Worker> m_hashWorker;
};

#endif