#include <Client.hpp>
#include <Config.hpp>
#include <Util/CURLEasy.hpp>
#include <Util/Logger.hpp>

bool QueuedRequest::operator<(const QueuedRequest& other) const {
    if(type != other.type) {
        return type < other.type;
    }

    if(title == nullptr && other.title != nullptr) {
        return true;
    }

    if(title != nullptr && other.title == nullptr) {
        return false;
    }

    if(title != nullptr && other.title != nullptr && title->id() != other.title->id()) {
        return title->id() < other.title->id();
    }

    return false;
}

bool QueuedRequest::operator==(const QueuedRequest& other) const {
    return type == other.type && title == other.title;
}

void Client::startQueueWorker() {
    if(m_valid) {
        m_requestWorker->start();
    }
}

void Client::stopQueueWorker() {
    if(m_valid) {
        m_requestWorker->waitForExit();
    }
}

void Client::queueAction(QueuedRequest request) {
    if(std::find(m_requestQueue.begin(), m_requestQueue.end(), request) != m_requestQueue.end()) {
        return;
    }

    {
        std::scoped_lock lock(m_requestMutex);
        m_requestQueue.emplace(request);
    }

    m_networkQueueChangedSignal(m_requestQueue.size(), m_processingQueueRequest);
}

void Client::queueWorkerMain() {
    s64 sleepMS               = 50;
    s64 pingIntervalMSOnline  = 2500;
    s64 pingIntervalMSOffline = 500;

    s64 intervalCurrentMS = pingIntervalMSOnline;

    while(!m_requestWorker->waitingForExit()) {
        if(intervalCurrentMS >= (serverOnline() ? pingIntervalMSOnline : pingIntervalMSOffline)) {
            if(!wifiEnabled()) {
                setOnline(false);
            }
            else {
                if(!serverOnline()) {
                    CURLEasy easy(CURLEasyOptions{
                        .url     = std::format("{}/v1/status", url()),
                        .noBody  = true,
                        .timeout = 2,
                    });

                    setOnline(easy.perform() == CURLE_OK && easy.statusCode() == 204);
                }
                else {
                    queueAction({ .type = QueuedRequest::RELOAD_TITLE_CACHE });
                }
            }

            intervalCurrentMS = 0;
        }

        for(auto it = m_requestQueue.begin(); it != m_requestQueue.end() && serverOnline() && !m_requestWorker->waitingForExit();) {
            const QueuedRequest& request = *it;

            m_progressCurrent = 0;
            m_progressMax     = 0;

            if(request.type != QueuedRequest::RELOAD_TITLE_CACHE) {
                if(!m_processRequests) {
                    it++;
                    continue;
                }

                aptSetHomeAllowed(false);
                aptSetSleepAllowed(false);
            }
            else {
                m_showRequestProgress = false;
            }

            m_activeRequest          = request;
            m_processingQueueRequest = true;
            m_networkQueueChangedSignal(m_requestQueue.size(), true);

            Result res;
            switch(request.type) {
            case QueuedRequest::UPLOAD_SAVE:
                m_requestStatus = std::format("Upload Save\n{}", request.title->longDescription());
                m_requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = upload(request.title, SAVE))) {
                    if(res == Client::emptyUploadError()) {
                        m_requestFailedSignal(std::format("No save files to upload\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to upload save {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        m_requestFailedSignal(std::format("Failed to Upload Save\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::DOWNLOAD_SAVE:
                m_requestStatus = std::format("Download Save\n{}", request.title->longDescription());
                m_requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = download(request.title, SAVE))) {
                    if(res == Client::emptyDownloadError()) {
                        m_requestFailedSignal(std::format("No save files to download\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to download save {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        m_requestFailedSignal(std::format("Failed to Download Save\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::UPLOAD_EXTDATA:
                m_requestStatus = std::format("Upload Ext\n{}", request.title->longDescription());
                m_requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = upload(request.title, EXTDATA))) {
                    if(res == Client::emptyUploadError()) {
                        m_requestFailedSignal(std::format("No extdata files to upload\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to upload extdata {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        m_requestFailedSignal(std::format("Failed to Upload Extdata\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::DOWNLOAD_EXTDATA:
                m_requestStatus = std::format("Download Ext\n{}", request.title->longDescription());
                m_requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = download(request.title, EXTDATA))) {
                    if(res == Client::emptyDownloadError()) {
                        m_requestFailedSignal(std::format("No extdata files to download\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to download extdata {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        m_requestFailedSignal(std::format("Failed to Download Extdata\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::RELOAD_TITLE_CACHE: loadTitleInfoCache(); break;
            default:                                break;
            }

            m_activeRequest          = std::nullopt;
            m_processingQueueRequest = false;
            intervalCurrentMS        = 0;

            if(request.type != QueuedRequest::RELOAD_TITLE_CACHE) {
                m_requestStatus = std::format("");
                m_requestStatusChangedSignal(m_requestStatus);

                aptSetHomeAllowed(true);
                aptSetSleepAllowed(true);
            }
            else {
                m_showRequestProgress = true;
            }

            {
                std::scoped_lock lock(m_requestMutex);
                it = m_requestQueue.erase(it);
            }

            m_networkQueueChangedSignal(m_requestQueue.size(), false);
        }

        if(!m_requestWorker->waitingForExit()) {
            svcSleepThread(sleepMS * static_cast<s64>(1e+6));
            intervalCurrentMS += sleepMS;
        }
    }
}