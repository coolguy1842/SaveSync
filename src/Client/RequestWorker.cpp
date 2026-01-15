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
    if(!m_valid) {
        return;
    }

    m_requestWorker->start();
}

void Client::stopQueueWorker(bool block) {
    if(!m_valid) {
        return;
    }

    m_requestWorker->signalShouldExit();
    m_requestCondVar.broadcast();

    if(block) {
        m_requestWorker->waitForExit();
    }
}

void Client::queueAction(QueuedRequest request) {
    if(!m_valid) {
        return;
    }

    {
        auto lock = m_requestCondVar.mutex().lock();
        if(!m_requestQueue.emplace(request).second) {
            return;
        }

        sendQueueChangedSignal();
    }

    m_requestCondVar.broadcast();
}

void Client::sendQueueChangedSignal() {
    if(!m_valid) {
        return;
    }

    networkQueueChangedSignal(m_requestQueue.size(), m_processingQueueRequest);
}

void Client::queueWorkerMain() {
    if(!m_valid) {
        return;
    }

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
            if(R_FAILED(loadTitleInfoCache()) || !serverOnline()) {
                if(m_requestWorker->waitingForExit()) {
                    return;
                }

                svcSleepThread(50 * static_cast<u64>(1e+6));
                continue;
            }

            if(m_requestWorker->waitingForExit()) {
                return;
            }
        }

        if(m_requestQueue.empty()) {
            constexpr s64 maxWaitMS = 2500;
            checkOnline             = m_requestCondVar.wait(maxWaitMS * static_cast<s64>(1e+6)) != 0;

            if(m_requestWorker->waitingForExit()) {
                return;
            }
            else if(!wifiEnabled()) {
                continue;
            }
        }

        if(checkOnline) {
            queueAction({ .type = QueuedRequest::RELOAD_TITLE_CACHE });
        }

        while(!m_requestQueue.empty()) {
            const QueuedRequest request = *m_requestQueue.begin();
            {
                auto lock = m_requestCondVar.mutex().lock();
                m_requestQueue.erase(m_requestQueue.begin());
            }

            m_progressCurrent = 0;
            m_progressMax     = 0;

            if(request.type != QueuedRequest::RELOAD_TITLE_CACHE) {
                if(!m_processRequests) {
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
            case QueuedRequest::UPLOAD:
                if(R_FAILED(res = upload(request.title))) {
                    if(res == Client::emptyUploadError() || res == Client::noFilesUploadError()) {
                        requestFailedSignal(std::format("No Files to Upload\n{}", request.title->name()));
                        break;
                    }
                    else if(res == Client::finalizeUploadError()) {
                        requestFailedSignal(std::format("Couldn't Finalize (part of?) the Upload\n{}", request.title->name()));
                        break;
                    }

                    Logger::warn("Request Worker", "Failed to Upload {:016X}", request.title->id());
                    Logger::warn("Request Worker", res);

                    m_processRequests = false;
                    requestFailedSignal(std::format("Failed to Upload\n{}", request.title->name()));
                }

                break;
            case QueuedRequest::DOWNLOAD: {
                if(R_FAILED(res = download(request.title))) {
                    if(res == Client::emptyDownloadError()) {
                        requestFailedSignal(std::format("No Files to Download\n{}", request.title->name()));
                        break;
                    }
                    else if(res == Client::finalizeDownloadError()) {
                        requestFailedSignal(std::format("Couldn't Finalize (part of?) the Download\n{}", request.title->name()));
                        break;
                    }

                    Logger::warn("Request Worker", "Failed to Download {:016X}", request.title->id());
                    Logger::warn("Request Worker", res);

                    m_processRequests = false;
                    requestFailedSignal(std::format("Failed to Download\n{}", request.title->name()));
                }

                break;
            }
            case QueuedRequest::RELOAD_TITLE_CACHE: loadTitleInfoCache(); break;
            default:                                break;
            }

            m_activeRequest          = std::nullopt;
            m_processingQueueRequest = false;

            if(request.type != QueuedRequest::RELOAD_TITLE_CACHE) {
                aptSetHomeAllowed(true);
                aptSetSleepAllowed(true);
            }
            else {
                m_showRequestProgress = true;
            }

            sendQueueChangedSignal();
        }
    }
}