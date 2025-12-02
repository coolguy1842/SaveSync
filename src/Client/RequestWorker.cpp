#include <Client.hpp>
#include <Config.hpp>
#include <Debug/Logger.hpp>
#include <Util/CURLEasy.hpp>

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
    auto lock = m_requestMutex.lock();
    if(m_stagingRequestQueue.emplace(request).second) {
        sendQueueChangedSignal();
        CondVar_Signal(&m_requestSignal);
    }
}

void Client::sendQueueChangedSignal() {
    networkQueueChangedSignal(m_stagingRequestQueue.size() + m_requestQueue.size(), m_processingQueueRequest);
}

void Client::queueWorkerMain() {
    bool checkOnline = false;
    while(!m_requestWorker->waitingForExit()) {
        if(!wifiEnabled()) {
            setOnline(false);
            while(!wifiEnabled()) {
                svcSleepThread(0);

                if(m_requestWorker->waitingForExit()) {
                    return;
                }
            }
        }

        if(!serverOnline()) {
            loadTitleInfoCache();

            if(!serverOnline() || m_requestWorker->waitingForExit()) {
                continue;
            }
        }

        {
            auto lock = m_requestMutex.lock();
            if(m_requestQueue.empty()) {
                constexpr s64 maxWaitMS = 2500;
                checkOnline             = CondVar_WaitTimeout(&m_requestSignal, m_requestMutex.native_handle(), maxWaitMS * static_cast<s64>(1e+6)) != 0;

                if(m_requestWorker->waitingForExit()) {
                    return;
                }
                else if(!wifiEnabled()) {
                    continue;
                }
            }
        }

        if(checkOnline) {
            queueAction({ .type = QueuedRequest::RELOAD_TITLE_CACHE });
        }

        auto lock = m_requestMutex.lock();
        m_requestQueue.swap(m_stagingRequestQueue);
        lock.release();

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
            sendQueueChangedSignal();

            Result res;
            switch(request.type) {
            case QueuedRequest::UPLOAD_SAVE:
                m_requestStatus = std::format("Upload Save\n{}", request.title->longDescription());
                requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = upload(request.title, SAVE))) {
                    if(res == Client::emptyUploadError()) {
                        requestFailedSignal(std::format("No save files to upload\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to upload save {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        requestFailedSignal(std::format("Failed to Upload Save\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::DOWNLOAD_SAVE:
                m_requestStatus = std::format("Download Save\n{}", request.title->longDescription());
                requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = download(request.title, SAVE))) {
                    if(res == Client::emptyDownloadError()) {
                        requestFailedSignal(std::format("No save files to download\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to download save {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        requestFailedSignal(std::format("Failed to Download Save\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::UPLOAD_EXTDATA:
                m_requestStatus = std::format("Upload Ext\n{}", request.title->longDescription());
                requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = upload(request.title, EXTDATA))) {
                    if(res == Client::emptyUploadError()) {
                        requestFailedSignal(std::format("No extdata files to upload\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to upload extdata {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        requestFailedSignal(std::format("Failed to Upload Extdata\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::DOWNLOAD_EXTDATA:
                m_requestStatus = std::format("Download Ext\n{}", request.title->longDescription());
                requestStatusChangedSignal(m_requestStatus);

                if(R_FAILED(res = download(request.title, EXTDATA))) {
                    if(res == Client::emptyDownloadError()) {
                        requestFailedSignal(std::format("No extdata files to download\n{}", request.title->longDescription()));
                    }
                    else {
                        Logger::warn("Request Worker", "Failed to download extdata {:X}", request.title->id());
                        Logger::warn("Request Worker", res);

                        m_processRequests = false;
                        requestFailedSignal(std::format("Failed to Download Extdata\n{}", request.title->longDescription()));
                    }
                }

                break;
            case QueuedRequest::RELOAD_TITLE_CACHE: loadTitleInfoCache(); break;
            default:                                break;
            }

            m_activeRequest          = std::nullopt;
            m_processingQueueRequest = false;

            if(request.type != QueuedRequest::RELOAD_TITLE_CACHE) {
                m_requestStatus = std::format("");
                requestStatusChangedSignal(m_requestStatus);

                aptSetHomeAllowed(true);
                aptSetSleepAllowed(true);
            }
            else {
                m_showRequestProgress = true;
            }

            it = m_requestQueue.erase(it);
            sendQueueChangedSignal();
        }

        m_requestQueue.clear();
    }
}