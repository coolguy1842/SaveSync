#include <Util/ScopedService.hpp>
#include <ext_ptmplays.h>
#include <stdio.h>

ScopedService::ScopedService(const std::function<Result()>& onInit, const std::function<Result()>& onDestruct)
    : m_valid(false)
    , m_result(MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NO_DATA))
    , m_destructFunc(onDestruct) {
    if(onInit != nullptr) {
        m_result = onInit();
        m_valid  = R_SUCCEEDED(m_result);
    }
}

ScopedService::~ScopedService() {
    if(m_valid && m_destructFunc != nullptr) {
        m_destructFunc();
    }
}

bool ScopedService::valid() const { return m_valid; }
Result ScopedService::result() const { return m_result; }

Services::AM::AM()
    : ScopedService([]() -> Result { return amInit(); }, []() -> Result { amExit(); return RL_SUCCESS; }) {}

Services::RomFS::RomFS()
    : ScopedService([]() -> Result { return romfsInit(); }, []() -> Result { romfsExit(); return RL_SUCCESS; }) {}

Services::PTMU::PTMU()
    : ScopedService([]() -> Result { return ptmuInit(); }, []() -> Result { ptmuExit(); return RL_SUCCESS; }) {}

Services::PTMPLAYS::PTMPLAYS()
    : ScopedService([]() -> Result { return ptmPlaysInit(); }, []() -> Result { ptmPlaysExit(); return RL_SUCCESS; }) {}

Services::MCUHWc::MCUHWc()
    : ScopedService([]() -> Result { return mcuHwcInit(); }, []() -> Result { mcuHwcExit(); return RL_SUCCESS; }) {}

Services::GFX::GFX(GSPGPU_FramebufferFormat topFormat, GSPGPU_FramebufferFormat bottomFormat, bool vrambuffers)
    : ScopedService([topFormat, bottomFormat, vrambuffers]() -> Result { gfxInit(topFormat, bottomFormat, vrambuffers); return RL_SUCCESS; }, []() -> Result { gfxExit(); return RL_SUCCESS; }) {}

Services::GFX::GFX()
    : ScopedService([]() -> Result { gfxInitDefault(); return RL_SUCCESS; }, []() -> Result { gfxExit(); return RL_SUCCESS; }) {}