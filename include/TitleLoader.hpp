#ifndef __TITLE_LOADER_HPP__
#define __TITLE_LOADER_HPP__

#include <3ds.h>

#include <Title.hpp>
#include <Util/Mutex.hpp>
#include <array>
#include <atomic>
#include <list>
#include <memory>
#include <optional>
#include <rocket.hpp>
#include <stack>
#include <vector>

#include <Util/CondVar.hpp>
#include <Util/Mutex.hpp>
#include <Util/ScopedService.hpp>
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

public:
    rocket::thread_safe_signal<void(const size_t&)> titlesLoadedChangedSignal;
    rocket::thread_safe_signal<void()> titlesFinishedLoadingSignal;
    rocket::thread_safe_signal<void(const std::shared_ptr<Title>&)> titleHashedSignal;

private:
    bool loadGameCardTitle();
    void loadSDTitles(u32 numTitles = 0);

    void cardWorkerMain();
    void loadWorkerMain();
    void hashWorkerMain();

    bool gameCardSupported();

private:
    Mutex m_titlesMutex;
    std::vector<std::shared_ptr<Title>> m_titles;

    struct TitleEntry {
        u64 id;
        FS_MediaType mediaType;
        FS_CardType cardType;
    };

    size_t m_totalTitles               = 0;
    std::atomic<size_t> m_titlesLoaded = 0;

    // title id for last pinged game cartridge
    u64 m_lastCardID;

    // watches for changes in the game card
    std::unique_ptr<Worker> m_cardWorker;
    std::unique_ptr<Worker> m_loaderWorker;
    std::unique_ptr<Worker> m_hashWorker;

    ConditionVariable m_condVar;
    Services::AM p_AM;
};

#endif