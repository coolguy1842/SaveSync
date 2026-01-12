#include <Client.hpp>
#include <Config.hpp>
#include <Debug/Logger.hpp>
#include <Util/CURLEasy.hpp>
#include <cstdlib>
#include <malloc.h>

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

Result Client::performFailError() { return MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, RD_NO_DATA); }
Result Client::invalidStatusCodeError() { return MAKERESULT(RL_PERMANENT, RS_INVALIDRESVAL, RM_APPLICATION, RD_INVALID_COMBINATION); }

bool Client::SOCInitialized = false;
u32* Client::SOCBuffer      = nullptr;
size_t Client::numClients   = 0;

bool Client::initSOC() {
    if(SOCInitialized) {
        return true;
    }

    Result res;
    CURLcode code;

    if(R_FAILED(res = acInit())) {
        Logger::critical("Client Init SOC", "Failed to init AC");
        Logger::critical("Client Init SOC", res);

        return false;
    }

    SOCBuffer = reinterpret_cast<u32*>(memalign(SOC_ALIGN, SOC_BUFFERSIZE));
    if(SOCBuffer == nullptr) {
        Logger::critical("Client Init SOC", "Failed to create SOCBuffer");

        goto cleanupAC;
    }

    if(R_FAILED(res = socInit(SOCBuffer, SOC_BUFFERSIZE))) {
        Logger::critical("Client Init SOC", "Failed to init SOC");
        Logger::critical("Client Init SOC", res);

        goto cleanupSOCBuf;
    }

    if((code = curl_global_init(CURL_GLOBAL_DEFAULT)) != CURLE_OK) {
        Logger::critical("Client Init SOC", "Failed to init curl {}", static_cast<int>(code));

        socExit();
    cleanupSOCBuf:
        if(SOCBuffer != nullptr) {
            free(SOCBuffer);
        }

    cleanupAC:
        acExit();
        return false;
    }

    SOCInitialized = true;
    return true;
}

void Client::closeSOC() {
    if(!SOCInitialized || numClients != 0) {
        return;
    }

    curl_global_cleanup();
    if(R_SUCCEEDED(socExit()) && SOCBuffer != nullptr) {
        free(SOCBuffer);
    }

    acExit();
    SOCInitialized = false;
}

Client::Client(std::string url)
    : m_valid(false)
    , m_url(url)
    , m_requestWorker(std::make_unique<Worker>([this](Worker*) { queueWorkerMain(); }, 6, 0x15000))
    , m_serverOnline(false)
    , m_titleInfoCached(false)
    , m_processRequests(true)
    , m_processingQueueRequest(false)
    , m_showRequestProgress(true)
    , m_progressCurrent(0)
    , m_progressMax(0) {
    if(!initSOC()) {
        return;
    }

    numClients++;
    m_valid = true;
}

Client::~Client() {
    networkQueueChangedSignal.clear();
    requestProgressChangedSignal.clear();
    titleCacheChangedSignal.clear();
    titleInfoChangedSignal.clear();
    requestFailedSignal.clear();

    if(!m_valid) {
        return;
    }

    stopQueueWorker();
    m_valid = false;

    if(numClients != 0) {
        numClients--;
    }

    closeSOC();
}

bool Client::valid() const { return m_valid; }
bool Client::wifiEnabled() {
    if(!m_valid) {
        return false;
    }

    u32 status = 0;
    if(R_FAILED(ACU_GetWifiStatus(&status))) {
        return false;
    }

    return status != AC_AP_TYPE_NONE;
}

bool Client::serverOnline() {
    if(!m_valid) {
        return false;
    }

    if(!wifiEnabled() && m_serverOnline) {
        setOnline(false);
    }

    return m_serverOnline;
}

void Client::setOnline(bool online) {
    if(m_serverOnline == online) {
        return;
    }

    m_serverOnline = online;

    if(online) {
        queueAction({ .type = QueuedRequest::RELOAD_TITLE_CACHE });
    }
    else {
        clearTitleInfoCache();
    }
}

bool Client::showRequestProgress() const { return m_showRequestProgress; }
bool Client::processingQueueRequest() const { return m_processingQueueRequest; }

bool Client::willProcessRequests() const { return m_processRequests; }
void Client::setProcessRequests(bool process) { m_processRequests = process; }

u64 Client::requestProgressCurrent() const { return m_progressCurrent; }
u64 Client::requestProgressMax() const { return m_progressMax; }

QueuedRequest Client::currentRequest() const { return m_activeRequest.value_or(QueuedRequest{ .type = QueuedRequest::NONE, .title = nullptr }); }

size_t Client::requestQueueSize() const { return m_requestQueue.size(); }
std::set<QueuedRequest> Client::requestQueue() const { return m_requestQueue; }

std::string Client::url() const { return m_url; }
void Client::setURL(std::string url) { m_url = url; }