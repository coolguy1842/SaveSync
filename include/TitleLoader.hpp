#ifndef __TITLE_LOADER_HPP__
#define __TITLE_LOADER_HPP__

#include <3ds.h>

#include <Title.hpp>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include <Util/Worker.hpp>
#include <sigs.h>

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

public:
    [[nodiscard]] auto titlesLoadedChangedSignal() { return m_titlesLoadedChangedSignal.interface(); }
    [[nodiscard]] auto titlesFinishedLoadingSignal() { return m_titlesFinishedLoadingSignal.interface(); }
    [[nodiscard]] auto titleHashedSignal() { return m_titleHashedSignal.interface(); }

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

    static constexpr size_t NumChunkWorkers = 3;
    std::array<Worker, NumChunkWorkers> m_chunkWorkers;
    std::array<std::stack<TitleEntry>, NumChunkWorkers> m_chunkWorkerTitles;

    bool m_SDTitlesLoaded;

    size_t m_totalTitles               = 0;
    std::atomic<size_t> m_titlesLoaded = 0;

    std::unique_ptr<Worker> m_loaderWorker;
    std::unique_ptr<Worker> m_hashWorker;

private:
    sigs::Signal<void(const size_t&)> m_titlesLoadedChangedSignal;
    sigs::Signal<void()> m_titlesFinishedLoadingSignal;
    sigs::Signal<void(const std::shared_ptr<Title>&, const Container&)> m_titleHashedSignal;
};

#endif