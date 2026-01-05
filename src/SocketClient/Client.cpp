#include <Debug/Logger.hpp>
#include <SocketClient.hpp>
#include <malloc.h>

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

static bool SOCInitialized = false;
static u32* SOCBuffer      = nullptr;

// only one client expected to run, multiple will cause issues, mostly with destructing
static Result initSOC() {
    if(SOCInitialized) {
        return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_ALREADY_EXISTS);
    }

    Result res;
    if(R_FAILED(res = acInit())) {
        Logger::critical("Client Init SOC", "Failed to init AC");
        Logger::critical("Client Init SOC", res);

        return res;
    }

    if((SOCBuffer = reinterpret_cast<u32*>(memalign(SOC_ALIGN, SOC_BUFFERSIZE))) == nullptr) {
        Logger::critical("Client Init SOC", "Failed to create SOCBuffer");
        Logger::critical("Client Init SOC", res);

        goto cleanupAC;
    }

    if(R_FAILED(res = socInit(SOCBuffer, SOC_BUFFERSIZE))) {
        Logger::critical("Client Init SOC", "Failed to init SOC");
        Logger::critical("Client Init SOC", res);

        if(SOCBuffer != nullptr) {
            free(SOCBuffer);
        }

    cleanupAC:
        acExit();
        return res;
    }

    SOCInitialized = true;
    return res;
}

static void closeSOC() {
    if(!SOCInitialized) {
        return;
    }

    if(R_SUCCEEDED(socExit()) && SOCBuffer != nullptr) {
        free(SOCBuffer);
    }

    acExit();
    SOCInitialized = false;
}

SocketClient::SocketClient()
    : m_valid(false)
    , m_url("192.168.1.4")
    , m_port(8000) {
    if(R_FAILED(m_result = initSOC())) {
        return;
    }

    m_valid = true;
    updateSocket();
}

SocketClient::~SocketClient() {
    if(!m_valid) {
        return;
    }

    closeSOC();
}

bool SocketClient::valid() const { return m_valid; }
Result SocketClient::lastResult() const { return m_result; }
bool SocketClient::wifiEnabled() {
    u32 out;
    return m_valid &&
           R_SUCCEEDED(m_result = ACU_GetWifiStatus(&out)) &&
           out != AC_AP_TYPE_NONE;
}