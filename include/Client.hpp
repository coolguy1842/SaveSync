#ifndef __CLIENT_HPP__
#define __CLIENT_HPP__

#include <3ds.h>
#include <curl/curl.h>

#include <Title.hpp>
#include <Util/CondVar.hpp>
#include <Util/Mutex.hpp>
#include <Util/Worker.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <rocket.hpp>
#include <set>

struct TitleInfo {
    std::vector<FileInfo> save;
    std::vector<FileInfo> extdata;
};

struct QueuedRequest {
    enum RequestType {
        NONE,

        UPLOAD,
        DOWNLOAD,

        RELOAD_TITLE_CACHE,
    };

    RequestType type;
    std::shared_ptr<Title> title = nullptr;

    bool operator<(const QueuedRequest& other) const;
    bool operator==(const QueuedRequest& other) const;
};

class Client {
public:
    Client(std::string url = "");
    ~Client();

    bool valid() const;

    std::string url() const;
    void setURL(std::string url);

    bool wifiEnabled();
    bool serverOnline();

    Result upload(std::shared_ptr<Title> title);
    Result download(std::shared_ptr<Title> title, Container container);

    std::unordered_map<u64, TitleInfo> cachedTitleInfo();
    bool cachedTitleInfoLoaded() const;

    void queueAction(QueuedRequest request);

    void startQueueWorker();
    void stopQueueWorker(bool block = true);

    bool processingQueueRequest() const;
    bool showRequestProgress() const;

    bool willProcessRequests() const;
    void setProcessRequests(bool process = true);

    u64 requestProgressCurrent() const;
    u64 requestProgressMax() const;

    QueuedRequest currentRequest() const;

    size_t requestQueueSize() const;
    std::set<QueuedRequest> requestQueue() const;

public:
    rocket::thread_safe_signal<void(const size_t&, const bool&)> networkQueueChangedSignal;
    rocket::thread_safe_signal<void(const u64&, const u64&)> requestProgressChangedSignal;
    rocket::thread_safe_signal<void()> titleCacheChangedSignal;
    rocket::thread_safe_signal<void(const u64&, const TitleInfo&)> titleInfoChangedSignal;
    rocket::thread_safe_signal<void(const std::string&)> requestFailedSignal;

public:
    static Result noFilesUploadError();
    static Result emptyUploadError();

    static Result finalizeUploadError();

    static Result emptyDownloadError();

private:
    void sendQueueChangedSignal();
    void queueWorkerMain();

    struct DownloadAction {
        enum Action {
            KEEP,
            REPLACE,
            CREATE,
            REMOVE
        };

        static std::string actionKey(Action type);
        // defaults to keep if invalid
        static Action actionValue(std::string type);

        std::string path;
        Action action;

        std::optional<u64> size;
        std::optional<std::string> hash;
    };

    // ticket is the identifier for the upload (uuidv4), will be overwritten with the output ticket
    Result beginUpload(std::shared_ptr<Title> title, Container container, std::string& ticket, std::set<std::string>& requestedFiles);
    Result beginDownload(std::shared_ptr<Title> title, Container container, std::string& ticket, std::vector<DownloadAction>& fileActions);

    Result uploadFile(const std::string& ticket, std::shared_ptr<File> file, const std::string& path);
    Result downloadFile(const std::string& ticket, std::shared_ptr<File> file, const std::string& path);

    Result endUpload(const std::string& ticket);
    Result endDownload(const std::string& ticket);

    Result cancelUpload(const std::string& ticket);

    Result loadTitleInfoCache();
    void clearTitleInfoCache();

private:
    Result performFailError();
    Result invalidStatusCodeError();

private:
    // returns if soc was/is initialized
    static bool initSOC();
    static void closeSOC();

    void setOnline(bool online = true);

    static bool SOCInitialized;
    static u32* SOCBuffer;
    static size_t numClients;

    bool m_valid;
    std::string m_url;

    std::unique_ptr<Worker> m_requestWorker;
    std::set<QueuedRequest> m_requestQueue;

    std::optional<QueuedRequest> m_activeRequest;
    ConditionVariable m_requestCondVar;

    Mutex m_cachedTitleInfoMutex;

    bool m_serverOnline;
    std::unordered_map<u64, TitleInfo> m_cachedTitleInfo;

    std::atomic<bool> m_titleInfoCached;

    bool m_processRequests;
    bool m_processingQueueRequest;
    bool m_showRequestProgress;

    u64 m_progressCurrent;
    u64 m_progressMax;
};

#endif